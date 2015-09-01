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

#include <openspace/util/time.h>
#include <openspace/util/spicemanager.h>

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
    const std::string KeyStartTime = "StartTime";
    const std::string KeyEndTime = "EndTime";
    const std::string GlslHelpersPath = "${MODULES}/multiresvolume/shaders/helpers_fs.glsl";
    const std::string GlslHelperPath = "${MODULES}/multiresvolume/shaders/helper.glsl";
    const std::string GlslHeaderPath = "${MODULES}/multiresvolume/shaders/header.glsl";
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
    , _atlasMapSize(0)
    , _tfBrickSelector(nullptr)
    , _simpleTfBrickSelector(nullptr)
    , _localTfBrickSelector(nullptr)
    , _errorHistogramManager(nullptr)
    , _histogramManager(nullptr)
    , _localErrorHistogramManager(nullptr)
    , _currentTime("currentTime", "Current Time", 0.f, 0.f, 1.f)
    , _memoryBudget("memoryBudget", "Memory Budget", 0.f, 0.f, 512.f)
    , _streamingBudget("streamingBudget", "Streaming Budget", 0.f, 0.f, 512.f)
    , _useGlobalTime("useGlobalTime", "Global Time", false)
    , _loop("loop", "Loop", false)
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

    std::string startTimeString, endTimeString;
    bool hasTimeData = true;
    hasTimeData &= dictionary.getValue(KeyStartTime, startTimeString);
    hasTimeData &= dictionary.getValue(KeyEndTime, endTimeString);
    if (hasTimeData) {
        hasTimeData &= SpiceManager::ref().getETfromDate(startTimeString, _startTime);
        hasTimeData &= SpiceManager::ref().getETfromDate(endTimeString, _endTime);
        if (!hasTimeData) {
            LERROR("Failed to convert times from config file");
        }
    }
    if (hasTimeData) {
        _loop = false;
    } else {
	_loop = true;
        LWARNING("Node " << name << " does not provide valid time information. Viewing one image per frame.");
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
    _selector = selector;

    addProperty(_useGlobalTime);
    addProperty(_loop);
    addProperty(_currentTime);
    addProperty(_memoryBudget);
    addProperty(_streamingBudget);

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

bool RenderableMultiresVolume::setSelectorType(Selector selector) {
    _selector = selector;
    switch (_selector) {
        case Selector::TF:
            if (!_tfBrickSelector) {
                TfBrickSelector* tbs;
                _errorHistogramManager = new ErrorHistogramManager(_tsp);
                _tfBrickSelector = tbs = new TfBrickSelector(_tsp, _errorHistogramManager, _transferFunction, _memoryBudget, _streamingBudget);
                _transferFunction->setCallback([tbs](const TransferFunction &tf) {
                    tbs->calculateBrickErrors();
                });
                return initializeSelector();
            }
            break;

        case Selector::SIMPLE:
            if (!_simpleTfBrickSelector) {
                SimpleTfBrickSelector *stbs;
                _histogramManager = new HistogramManager();
                _simpleTfBrickSelector = stbs = new SimpleTfBrickSelector(_tsp, _histogramManager, _transferFunction, _memoryBudget);
                _transferFunction->setCallback([stbs](const TransferFunction &tf) {
                    stbs->calculateBrickImportances();
                });
                return initializeSelector();
            }
            break;

        case Selector::LOCAL:
            if (!_localTfBrickSelector) {
                LocalTfBrickSelector* ltbs;
                _localErrorHistogramManager = new LocalErrorHistogramManager(_tsp);
                _localTfBrickSelector = ltbs = new LocalTfBrickSelector(_tsp, _localErrorHistogramManager, _transferFunction, _memoryBudget);
                _transferFunction->setCallback([ltbs](const TransferFunction &tf) {
                    ltbs->calculateBrickErrors();
                });
                return initializeSelector();
            }
            break;
    }
    return false;
}

bool RenderableMultiresVolume::initialize() {
    bool success = RenderableVolume::initialize();

    success &= _tsp && _tsp->load();
    _memoryBudget = 512;
    _streamingBudget = 512;

    if (success) {
        _brickIndices.resize(_tsp->header().xNumBricks_ * _tsp->header().yNumBricks_ * _tsp->header().zNumBricks_, 0);
        success &= setSelectorType(_selector);
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
    int numTimesteps = _tsp->header().numTimesteps_;
    int currentTimestep;
    bool visible = true;
    if (_loop) {
        currentTimestep = _timestep % numTimesteps;
    } else if (_useGlobalTime) {
        double t = (_time - _startTime) / (_endTime - _startTime);
        currentTimestep = t * numTimesteps;
        visible = currentTimestep >= 0 && currentTimestep < numTimesteps;
    } else {
	currentTimestep = _currentTime * numTimesteps;
    }

    if (visible) {
        switch (_selector) {
        case Selector::TF:
            if (_tfBrickSelector) {
                _tfBrickSelector->setMemoryBudget(_memoryBudget);
                _tfBrickSelector->setStreamingBudget(_streamingBudget);
                _tfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
        case Selector::SIMPLE:
            if (_simpleTfBrickSelector) {
                _simpleTfBrickSelector->setBrickBudget(_memoryBudget);
                _simpleTfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
        case Selector::LOCAL:
            if (_localTfBrickSelector) {
                _localTfBrickSelector->setBrickBudget(_memoryBudget);
                _localTfBrickSelector->selectBricks(currentTimestep, _brickIndices);
            }
            break;
        }
        _atlasManager->updateAtlas(AtlasManager::EVEN, _brickIndices);
    }

    std::stringstream ss;
    ss << "opacity_" << getId();
    program->setUniform(ss.str(), visible ? 1.0f : 0.0f);

    ss.str(std::string());
    ss << "transferFunction_" << getId();
    program->setUniform(ss.str(), getTextureUnit(_transferFunction->getTexture()));

    ss.str(std::string());
    ss << "textureAtlas_" << getId();
    program->setUniform(ss.str(), getTextureUnit(_atlasManager->textureAtlas()));

    ss.str(std::string());
    ss << "atlasMapBlock_" << getId();
    program->setSsboBinding(ss.str(), getSsboBinding(_atlasManager->atlasMapBuffer()));

    ss.str(std::string());
    ss << "gridType_" << getId();
    program->setUniform(ss.str(), static_cast<int>(_tsp->header().gridType_));

    ss.str(std::string());
    ss << "maxNumBricksPerAxis_" << getId();
    program->setUniform(ss.str(), static_cast<unsigned int>(_tsp->header().xNumBricks_));

    ss.str(std::string());
    ss << "paddedBrickDim_" << getId();
    program->setUniform(ss.str(), static_cast<unsigned int>(_tsp->paddedBrickDim()));

    ss.str(std::string());
    ss << "atlasSize_" << getId();
    glm::size3_t size = _atlasManager->textureSize();
    glm::ivec3 atlasSize(size.x, size.y, size.z);
    program->setUniform(ss.str(), atlasSize);

    _timestep++;
}

std::string RenderableMultiresVolume::getHelperPath() {
    return absPath(GlslHelperPath);
}

std::string RenderableMultiresVolume::getHeaderPath() {
    return absPath(GlslHeaderPath);
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
    _time = data.time;
    RenderableVolume::update(data);
}

} // namespace openspace
