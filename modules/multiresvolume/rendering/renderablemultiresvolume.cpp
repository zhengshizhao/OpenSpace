/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2015                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/multiresvolume/rendering/renderablemultiresvolume.h>

#include <openspace/engine/configurationmanager.h>
#include <openspace/engine/openspaceengine.h>
#include <modules/kameleon/include/kameleonwrapper.h>
#include <openspace/util/constants.h>
#include <openspace/abuffer/abuffer.h>
#include <openspace/rendering/renderengine.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/filesystem/cachemanager.h>

#include <ghoul/opengl/framebufferobject.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/texture.h>

#include <modules/multiresvolume/rendering/tsp.h>
#include <modules/multiresvolume/rendering/atlasmanager.h>
#include <modules/multiresvolume/rendering/shenbrickselector.h>
#include <modules/multiresvolume/rendering/tfbrickselector.h>
#include <modules/multiresvolume/rendering/simpletfbrickselector.h>
#include <modules/multiresvolume/rendering/localtfbrickselector.h>

#include <modules/multiresvolume/rendering/histogrammanager.h>
#include <modules/multiresvolume/rendering/errorhistogrammanager.h>
#include <modules/multiresvolume/rendering/localerrorhistogrammanager.h>

#include <algorithm>
#include <iterator>

namespace {
    const std::string _loggerCat = "RenderableMultiresVolume";
    const std::string KeyDataSource = "Source";
    const std::string KeyHints = "Hints";
    const std::string KeyTransferFunction = "TransferFunction";
    const std::string KeySampler = "Sampler";
    const std::string KeyBoxScaling = "BoxScaling";
    const std::string KeyModelName = "ModelName";
    const std::string KeyVolumeName = "VolumeName";
    const std::string KeyBrickSelector = "BrickSelector";
    const std::string GlslHelpersPath = "${MODULES}/multiresvolume/shaders/helpers_fs.glsl";
    bool registeredGlslHelpers = false;
}

namespace openspace {

RenderableMultiresVolume::RenderableMultiresVolume (const ghoul::Dictionary& dictionary)
    : RenderableVolume(dictionary)
    , _volumeName("")
    , _transferFunction(nullptr)
    , _spatialTolerance(1.000001)
    , _temporalTolerance(0.000001)
    , _timestep(0)
    , _brickBudget(512)
    , _atlasMapSize(0)
    , _tfBrickSelector(nullptr)
    , _simpleTfBrickSelector(nullptr)
    , _localTfBrickSelector(nullptr)
    , _errorHistogramManager(nullptr)
    , _histogramManager(nullptr)
    , _localErrorHistogramManager(nullptr)
{
    std::string name;
    bool success = dictionary.getValue(constants::scenegraphnode::keyName, name);
    assert(success);

    _filename = "";
    success = dictionary.getValue(KeyDataSource, _filename);
    if (!success) {
        LERROR("Node '" << name << "' did not contain a valid '" <<  KeyDataSource << "'");
        return;
    }
    _filename = absPath(_filename);
    if (_filename == "") {
        return;
    }

    _transferFunction = nullptr;
    _transferFunctionPath = "";
    success = dictionary.getValue(KeyTransferFunction, _transferFunctionPath);
    if (!success) {
        LERROR("Node '" << name << "' did not contain a valid '" <<
            KeyTransferFunction << "'");
        return;
    }
    _transferFunctionPath = absPath(_transferFunctionPath);
    _transferFunction = new TransferFunction(_transferFunctionPath);

    _pscOffset = psc(glm::vec4(0.0));
    _boxScaling = glm::vec3(1.0);

    if (dictionary.hasKey(KeyModelName)) {
        success = dictionary.getValue(KeyModelName, _modelName);
    }

    if (dictionary.hasKey(KeyBoxScaling)) {
        glm::vec4 scalingVec4(_boxScaling, _w);
        success = dictionary.getValue(KeyBoxScaling, scalingVec4);
        if (success) {
            _boxScaling = scalingVec4.xyz;
            _w = scalingVec4.w;
        }
        else {
            success = dictionary.getValue(KeyBoxScaling, _boxScaling);
            if (!success) {
                LERROR("Node '" << name << "' did not contain a valid '" <<
                    KeyBoxScaling << "'");
                return;
            }
        }
    }

    setBoundingSphere(PowerScaledScalar::CreatePSS(glm::length(_boxScaling)*pow(10,_w)));

    _tsp = new TSP(_filename);
    _atlasManager = new AtlasManager(_tsp);


    std::string brickSelectorType;
    Selector selector = Selector::TF;
    if (dictionary.hasKey(KeyBrickSelector)) {
        success = dictionary.getValue(KeyBrickSelector, brickSelectorType);
        if (success) {
            if (brickSelectorType == "tf") {
                selector = Selector::TF;
            } else if (brickSelectorType == "simple") {
                selector = Selector::SIMPLE;
            } else if (brickSelectorType == "local") {
                selector = Selector::LOCAL;
            }
        }
    }
    setSelectorType(selector);
    //_brickSelector = new ShenBrickSelector(_tsp, -1, -1);
}

RenderableMultiresVolume::~RenderableMultiresVolume() {
    //OsEng.renderEngine()->aBuffer()->removeVolume(this);
    if (_tsp)
        delete _tsp;
    if (_atlasManager)
        delete _atlasManager;

    if (_tfBrickSelector)
        delete _tfBrickSelector;
    if (_simpleTfBrickSelector)
        delete _simpleTfBrickSelector;
    if (_localTfBrickSelector)
        delete _localTfBrickSelector;

    if (_errorHistogramManager)
        delete _errorHistogramManager;
    if (_histogramManager)
        delete _histogramManager;
    if (_localErrorHistogramManager)
        delete _localErrorHistogramManager;

    if (_transferFunction)
        delete _transferFunction;
}

void RenderableMultiresVolume::setSelectorType(Selector selector) {
    _selector = selector;
    switch (_selector) {
        case Selector::TF:
            if (!_tfBrickSelector) {
                TfBrickSelector* tbs;
                _errorHistogramManager = new ErrorHistogramManager(_tsp);
                _tfBrickSelector = tbs = new TfBrickSelector(_tsp, _errorHistogramManager, _transferFunction, _brickBudget);
                _transferFunction->setCallback([tbs](const TransferFunction &tf) {
                    tbs->calculateBrickErrors();
                });
                initializeSelector();
            }
            break;

        case Selector::SIMPLE:
            if (!_simpleTfBrickSelector) {
                SimpleTfBrickSelector *stbs;
                _histogramManager = new HistogramManager();
                _simpleTfBrickSelector = stbs = new SimpleTfBrickSelector(_tsp, _histogramManager, _transferFunction, _brickBudget);
                _transferFunction->setCallback([stbs](const TransferFunction &tf) {
                    stbs->calculateBrickImportances();
                });
                initializeSelector();
            }
            break;

        case Selector::LOCAL:
            if (!_localTfBrickSelector) {
                LocalTfBrickSelector* ltbs;
                _localErrorHistogramManager = new LocalErrorHistogramManager(_tsp);
                _localTfBrickSelector = ltbs = new LocalTfBrickSelector(_tsp, _localErrorHistogramManager, _transferFunction, _brickBudget);
                _transferFunction->setCallback([ltbs](const TransferFunction &tf) {
                    ltbs->calculateBrickErrors();
                });
                initializeSelector();
            }
            break;
    }
}

bool RenderableMultiresVolume::initialize() {
    bool success = RenderableVolume::initialize();

    if (!registeredGlslHelpers) {
        OsEng.renderEngine()->aBuffer()->registerGlslHelpers(RenderableMultiresVolume::getGlslHelpers());
        registeredGlslHelpers = true;
    }

    success &= _tsp && _tsp->load();

    if (success) {
        _brickIndices.resize(_tsp->header().xNumBricks_ * _tsp->header().yNumBricks_ * _tsp->header().zNumBricks_, 0);
        success &= initializeSelector();
    }

    success &= _atlasManager && _atlasManager->initialize();

    _transferFunction->update();

    success &= isReady();

    return success;
}

bool RenderableMultiresVolume::deinitialize() {
    if (_tsp)
        delete _tsp;
    if (_transferFunction)
        delete _transferFunction;
    _tsp = nullptr;
    _transferFunction = nullptr;

    return RenderableVolume::deinitialize();
}

bool RenderableMultiresVolume::isReady() const {
    return true;
}

std::string RenderableMultiresVolume::getGlslHelpers() {
    std::ifstream f(absPath(GlslHelpersPath));
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    return str;
}

bool RenderableMultiresVolume::initializeSelector() {
    int nHistograms = 500;
    bool success = true;

    switch (_selector) {
        case Selector::TF:
            if (_errorHistogramManager) {
                std::stringstream cacheName;
                ghoul::filesystem::File f = _filename;
                cacheName << f.baseName() << "_" << nHistograms << "_errorHistograms";
                std::string cacheFilename;
                FileSys.cacheManager()->getCachedFile(cacheName.str(), "", cacheFilename, true);
                std::ifstream cacheFile(cacheFilename, std::ios::in | std::ios::binary);
                if (cacheFile.is_open()) {
                    // Read histograms from cache.
                    cacheFile.close();
                    LINFO("Loading histograms from " << cacheFilename);
                    success &= _errorHistogramManager->loadFromFile(cacheFilename);
                } else {
                    // Build histograms from tsp file.
                    LWARNING("Failed to open " << cacheFilename);
                    if (success &= _errorHistogramManager->buildHistograms(nHistograms)) {
                        LINFO("Writing cache to " << cacheFilename);
                        _errorHistogramManager->saveToFile(cacheFilename);
                    }
                }
                success &= _tfBrickSelector && _tfBrickSelector->initialize();
            }
            break;

        case Selector::SIMPLE:
            if (_histogramManager) {
                std::stringstream cacheName;
                ghoul::filesystem::File f = _filename;
                cacheName << f.baseName() << "_" << nHistograms << "_histograms";
                std::string cacheFilename;
                FileSys.cacheManager()->getCachedFile(cacheName.str(), "", cacheFilename, true);
                std::ifstream cacheFile(cacheFilename, std::ios::in | std::ios::binary);
                if (cacheFile.is_open()) {
                    // Read histograms from cache.
                    cacheFile.close();
                    LINFO("Loading histograms from " << cacheFilename);
                    success &= _histogramManager->loadFromFile(cacheFilename);
                } else {
                    // Build histograms from tsp file.
                    LWARNING("Failed to open " << cacheFilename);
                    if (success &= _histogramManager->buildHistograms(_tsp, nHistograms)) {
                        LINFO("Writing cache to " << cacheFilename);
                        _histogramManager->saveToFile(cacheFilename);
                    }
                }
                success &= _simpleTfBrickSelector && _simpleTfBrickSelector->initialize();
            }
            break;

        case Selector::LOCAL:
            if (_localErrorHistogramManager) {
                std::stringstream cacheName;
                ghoul::filesystem::File f = _filename;
                cacheName << f.baseName() << "_" << nHistograms << "_localErrorHistograms";
                std::string cacheFilename;
                FileSys.cacheManager()->getCachedFile(cacheName.str(), "", cacheFilename, true);
                std::ifstream cacheFile(cacheFilename, std::ios::in | std::ios::binary);
                if (cacheFile.is_open()) {
                    // Read histograms from cache.
                    cacheFile.close();
                    LINFO("Loading histograms from " << cacheFilename);
                    success &= _localErrorHistogramManager->loadFromFile(cacheFilename);
                } else {
                    // Build histograms from tsp file.
                    LWARNING("Failed to open " << cacheFilename);
                    if (success &= _localErrorHistogramManager->buildHistograms(nHistograms)) {
                        LINFO("Writing cache to " << cacheFilename);
                        _localErrorHistogramManager->saveToFile(cacheFilename);
                    }
                }
                success &= _localTfBrickSelector && _localTfBrickSelector->initialize();
            }
            break;
    }

    return success;
}

void RenderableMultiresVolume::preResolve(ghoul::opengl::ProgramObject* program) {
    const int numTimesteps = _tsp->header().numTimesteps_;
    const int currentTimestep = _timestep % numTimesteps;

    switch (_selector) {
        case Selector::TF:
            if (_tfBrickSelector) {
                _tfBrickSelector->setBrickBudget(_brickBudget);
                _tfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
        case Selector::SIMPLE:
            if (_simpleTfBrickSelector) {
                _simpleTfBrickSelector->setBrickBudget(_brickBudget);
                _simpleTfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
        case Selector::LOCAL:
            if (_localTfBrickSelector) {
                _localTfBrickSelector->setBrickBudget(_brickBudget);
                _localTfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
    }

    _atlasManager->updateAtlas(AtlasManager::EVEN, _brickIndices);

    program->setUniform(getGlslName("transferFunction"), getTextureUnit(_transferFunction->getTexture()));
    program->setUniform(getGlslName("textureAtlas"), getTextureUnit(_atlasManager->textureAtlas()));
    program->setSsboBinding(getGlslName("atlasMapBlock"), getSsboBinding(_atlasManager->atlasMapBuffer()));

    program->setUniform(getGlslName("gridType"), static_cast<int>(_tsp->header().gridType_));
    program->setUniform(getGlslName("maxNumBricksPerAxis"), static_cast<unsigned int>(_tsp->header().xNumBricks_));
    program->setUniform(getGlslName("paddedBrickDim"), static_cast<unsigned int>(_tsp->paddedBrickDim()));

    _timestep++;
}

std::string RenderableMultiresVolume::getSampler(const std::string& functionName) {
    std::stringstream ss;

    ss << "vec4 " << functionName << "(vec3 samplePos, vec3 dir, "
       << "float occludingAlpha, inout float maxStepSize) {" << std::endl
       << "    if (" << getGlslName("gridType") << " == 1) {" << std::endl
       << "        samplePos = multires_cartesianToSpherical(samplePos);" << std::endl
       << "    }" << std::endl
       << "    vec3 sampleCoords = " << getGlslName("atlasCoordsFunction") << "(samplePos);" << std::endl

       << "    float intensity = texture(" << getGlslName("textureAtlas") << ", sampleCoords).x;" << std::endl
       << "    vec4 contribution = texture(" << getGlslName("transferFunction") << ", intensity);" << std::endl
       << "    maxStepSize = 1.0/float(" << getGlslName("maxNumBricksPerAxis") << ")/float(" << getGlslName("paddedBrickDim") << ");" << std::endl
       << "    return contribution;" << std::endl
       << "}" << std::endl;
    // TODO: Calculate maximum step size as distance to next voxel
    return ss.str();
}

std::string RenderableMultiresVolume::getStepSizeFunction(const std::string& functionName) {
    std::stringstream ss;
    ss << "float " << functionName << "(vec3 samplePos, vec3 dir) {" << std::endl
       << "    return 1.0/float(" << getGlslName("maxNumBricksPerAxis") << ")/float(" << getGlslName("paddedBrickDim") << ");" << std::endl
       << "}" << std::endl;

    // TODO: Calculate maximum step size as distance to next voxel
    return ss.str();
}

std::string RenderableMultiresVolume::getHeader() {
    std::stringstream ss;
    ss << "uniform sampler1D " << getGlslName("transferFunction") << ";" << std::endl
       << "uniform sampler3D " << getGlslName("textureAtlas") << ";" << std::endl
       << "uniform int " << getGlslName("gridType") << ";" << std::endl
       << "uniform uint " << getGlslName("maxNumBricksPerAxis") << ";" << std::endl
       << "uniform uint " << getGlslName("paddedBrickDim") << ";" << std::endl

       << "layout (shared) buffer " << getGlslName("atlasMapBlock") << " {" << std::endl
       << "    uint " << getGlslName("atlasMap") << "[];" << std::endl
       << "};" << std::endl;

    ss << "void " << getGlslName("atlasMapDataFunction") << "(ivec3 brickCoords, inout uint atlasIntCoord, inout uint level) {" << std::endl
       << "    int linearBrickCoord = multires_intCoord(brickCoords, ivec3(" << getGlslName("maxNumBricksPerAxis") << "));" << std::endl
       << "    uint mapData = " << getGlslName("atlasMap") << "[linearBrickCoord];" << std::endl
       << "    level = mapData >> 28;" << std::endl
       << "    atlasIntCoord = mapData & 0x0FFFFFFF;" << std::endl
       << "}" << std::endl;

    ss << "vec3 " << getGlslName("atlasCoordsFunction") << "(vec3 position) {" << std::endl
       << "    uint maxNumBricksPerAxis = " << getGlslName("maxNumBricksPerAxis") << ";" << std::endl
       << "    uint paddedBrickDim = " << getGlslName("paddedBrickDim") << ";" << std::endl

       << "    ivec3 brickCoords = ivec3(position * maxNumBricksPerAxis);" << std::endl
       << "    uint atlasIntCoord, level;" << std::endl
       << "    " << getGlslName("atlasMapDataFunction") << "(brickCoords, atlasIntCoord, level);" << std::endl

       << "    float levelDim = float(maxNumBricksPerAxis) / pow(2.0, level);" << std::endl
       << "    vec3 inBrickCoords = mod(position*levelDim, 1.0);" << std::endl

       << "    float scale = float(paddedBrickDim) - 2.0;" << std::endl
       << "    vec3 paddedInBrickCoords = (1.0 + inBrickCoords * scale) / paddedBrickDim;" << std::endl

       << "    ivec3 numBricksInAtlas = textureSize(" << getGlslName("textureAtlas") << ", 0) / int(paddedBrickDim);" << std::endl
       << "    vec3 atlasOffset = multires_vec3Coords(atlasIntCoord, numBricksInAtlas);" << std::endl

       << "    return (atlasOffset + paddedInBrickCoords) / vec3(numBricksInAtlas);" << std::endl
       << "}" << std::endl;

    return ss.str();
}

std::vector<ghoul::opengl::Texture*> RenderableMultiresVolume::getTextures() {
    std::vector<ghoul::opengl::Texture*> textures{_transferFunction->getTexture(), _atlasManager->textureAtlas()};
    return textures;
}

std::vector<int> RenderableMultiresVolume::getBuffers() {
    std::vector<int> buffers{_atlasManager->atlasMapBuffer()};
    return buffers;
}

void RenderableMultiresVolume::update(const UpdateData& data) {
    RenderableVolume::update(data);
}

} // namespace openspace
