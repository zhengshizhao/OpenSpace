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
    , _brickBudget(512)
    , _atlasMapSize(0)
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


    //LocalTfBrickSelector* bs;
    //_localErrorHistogramManager = new LocalErrorHistogramManager(_tsp);
    //_brickSelector = bs = new LocalTfBrickSelector(_tsp, _localErrorHistogramManager, _transferFunction, _brickBudget);
    //_transferFunction->setCallback([bs](const TransferFunction &tf) {
    //    bs->calculateBrickErrors();
    //});
    
    TfBrickSelector* bs;
    _errorHistogramManager = new ErrorHistogramManager(_tsp);
    _brickSelector = bs = new TfBrickSelector(_tsp, _errorHistogramManager, _transferFunction, _brickBudget);
    _transferFunction->setCallback([bs](const TransferFunction &tf) {
        bs->calculateBrickErrors();
    });

    //SimpleTfBrickSelector *bs;
    //_brickSelector = bs = new SimpleTfBrickSelector(_tsp, _histogramManager, _transferFunction, _brickBudget);
    //_brickSelector = new ShenBrickSelector(_tsp, -1, -1);
    //_transferFunction->setCallback([bs](const TransferFunction &tf) {
    //    bs->calculateBrickImportances();
    //});
    //_brickSelector = new ShenBrickSelector(_tsp, -1, -1);
}

RenderableMultiresVolume::~RenderableMultiresVolume() {
    //OsEng.renderEngine()->aBuffer()->removeVolume(this);
    if (_tsp)
        delete _tsp;
    if (_atlasManager)
        delete _atlasManager;
    if (_brickSelector)
        delete _brickSelector;
    if (_localErrorHistogramManager)
        delete _localErrorHistogramManager;
    if (_errorHistogramManager)
        delete _errorHistogramManager;
    if (_histogramManager)
        delete _histogramManager;
    if (_transferFunction)
        delete _transferFunction;
}

bool RenderableMultiresVolume::initialize() {
    bool success = RenderableVolume::initialize();

    success &= _tsp && _tsp->load();

    if (success) {
        _brickIndices.resize(_tsp->header().xNumBricks_ * _tsp->header().yNumBricks_ * _tsp->header().zNumBricks_, 0);
 
    	//success &= _localErrorHistogramManager->buildHistograms(500);
        success &= _errorHistogramManager->buildHistograms(500);
    	//success &= _histogramManager->buildHistograms(tsp, 500);

    	/*
        // TODO: Cache data for error histogram manager.

        int nHistograms = 500;
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
            if (success &= _histogramManager->buildHistograms(_tsp, 500)) {
                LINFO("Writing cache to " << cacheFilename);
                _histogramManager->saveToFile(cacheFilename);
            }
        }
    	*/
    }

    success &= _atlasManager && _atlasManager->initialize();
    success &= _brickSelector && _brickSelector->initialize();
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


void RenderableMultiresVolume::preResolve(ghoul::opengl::ProgramObject* program) {
    const int numTimesteps = _tsp->header().numTimesteps_;
    const int currentTimestep = _timestep % numTimesteps;

    if (TfBrickSelector* tfbs = dynamic_cast<TfBrickSelector*>(_brickSelector)) {
        tfbs->setBrickBudget(_brickBudget);
    }

    if (SimpleTfBrickSelector* stfbs = dynamic_cast<SimpleTfBrickSelector*>(_brickSelector)) {
        stfbs->setBrickBudget(_brickBudget);
    }

<<<<<<< HEAD
=======
    if (LocalTfBrickSelector* ltfbs = dynamic_cast<LocalTfBrickSelector*>(_brickSelector)) {
        ltfbs->setBrickBudget(_brickBudget);
    }
>>>>>>> 4fd467d02d5c51b4704b7be1994a03c48ca4989a

    _brickSelector->selectBricks(currentTimestep, _brickIndices);

    _atlasManager->updateAtlas(AtlasManager::EVEN, _brickIndices);

<<<<<<< HEAD
=======
    program->setUniform(getGlslName("transferFunction"), getTextureUnit(_transferFunction->getTexture()));
    program->setUniform(getGlslName("textureAtlas"), getTextureUnit(_atlasManager->textureAtlas()));
    program->setSsboBinding(getGlslName("atlasMapBlock"), getSsboBinding(_atlasManager->atlasMapBuffer()));

    program->setUniform(getGlslName("gridType"), static_cast<int>(_tsp->header().gridType_));
    program->setUniform(getGlslName("maxNumBricksPerAxis"), static_cast<unsigned int>(_tsp->header().xNumBricks_));
    program->setUniform(getGlslName("paddedBrickDim"), static_cast<unsigned int>(_tsp->paddedBrickDim()));

    _timestep++;
}

std::string RenderableMultiresVolume::getSampler(const std::string& functionName) {
>>>>>>> 4fd467d02d5c51b4704b7be1994a03c48ca4989a
    std::stringstream ss;
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
    RenderableVolume::update(data);
}

} // namespace openspace
