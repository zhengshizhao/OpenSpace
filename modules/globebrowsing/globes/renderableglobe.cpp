/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2016                                                               *
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

#include <modules/globebrowsing/globes/renderableglobe.h>
#include <modules/globebrowsing/globes/globemesh.h>
#include <modules/globebrowsing/other/threadpool.h>
#include <modules/globebrowsing/other/temporaltileprovider.h>

// open space includes
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/spicemanager.h>
#include <openspace/scene/scenegraphnode.h>

// ghoul includes
#include <ghoul/misc/assert.h>

namespace {
    const std::string _loggerCat = "RenderableGlobe";

    // Keys for the dictionary
    const std::string keyRadii = "Radii";
    const std::string keySegmentsPerPatch = "SegmentsPerPatch";
    const std::string keyTextures = "Textures";
    const std::string keyColorTextures = "ColorTextures";
    const std::string keyHeightMaps = "HeightMaps";
}



namespace openspace {


    RenderableGlobe::RenderableGlobe(const ghoul::Dictionary& dictionary)
        : _saveOrThrowCamera(properties::BoolProperty("saveOrThrowCamera", "saveOrThrowCamera"))
        , doFrustumCulling(properties::BoolProperty("doFrustumCulling", "doFrustumCulling"))
        , doHorizonCulling(properties::BoolProperty("doHorizonCulling", "doHorizonCulling"))
        , mergeInvisible(properties::BoolProperty("mergeInvisible", "mergeInvisible", true))
        , lodScaleFactor(properties::FloatProperty("lodScaleFactor", "lodScaleFactor", 5.0f, 1.0f, 20.0f))
        , initChunkVisible(properties::BoolProperty("initChunkVisible", "initChunkVisible", true))
        , renderSmallChunksFirst(properties::BoolProperty("renderSmallChunksFirst", "renderSmallChunksFirst", true))
        , useHeightMap(properties::BoolProperty("useHeightMap", "useHeightMap", true))
        , useColorMap(properties::BoolProperty("useColorMap", "useColorMap", true))
        , blendHeightMap(properties::BoolProperty("blendHeightMap", "blendHeightMap", true))
        , blendColorMap(properties::BoolProperty("blendColorMap", "blendColorMap", true))
    {
        setName("RenderableGlobe");
        
        addProperty(_saveOrThrowCamera);
        addProperty(doFrustumCulling);
        addProperty(doHorizonCulling);
        addProperty(mergeInvisible);
        addProperty(lodScaleFactor);
        addProperty(initChunkVisible);
        addProperty(renderSmallChunksFirst);

        addProperty(useHeightMap);
        addProperty(useColorMap);
        addProperty(blendHeightMap);
        addProperty(blendColorMap);

        doFrustumCulling.setValue(true);
        doHorizonCulling.setValue(true);
        renderSmallChunksFirst.setValue(true);

        // Read the radii in to its own dictionary
        Vec3 radii;
        dictionary.getValue(keyRadii, radii);
        _ellipsoid = Ellipsoid(radii);
        setBoundingSphere(pss(_ellipsoid.averageRadius(), 0.0));

        // Ghoul can't read ints from lua dictionaries
        double patchSegmentsd;
        dictionary.getValue(keySegmentsPerPatch, patchSegmentsd);
        int patchSegments = patchSegmentsd;
        
        // Init tile provider manager
        ghoul::Dictionary texturesDictionary;
        dictionary.getValue(keyTextures, texturesDictionary);
        _tileProviderManager = std::shared_ptr<TileProviderManager>(
            new TileProviderManager(texturesDictionary));

        auto colorProviders = _tileProviderManager->colorTextureProviders();
        auto heightProviders = _tileProviderManager->heightMapProviders();

        addToggleLayerProperties(colorProviders, _activeColorLayers);
        addToggleLayerProperties(heightProviders, _activeHeightMapLayers);

        _chunkedLodGlobe = std::shared_ptr<ChunkedLodGlobe>(
            new ChunkedLodGlobe(_ellipsoid, patchSegments, _tileProviderManager));

        _distanceSwitch.addSwitchValue(_chunkedLodGlobe, 1e12);
    }

    RenderableGlobe::~RenderableGlobe() {

    }

    void RenderableGlobe::addToggleLayerProperties(
        std::vector<TileProviderManager::TileProviderWithName>& tileProviders,
        std::vector<properties::BoolProperty>& dest)
    {
        for (size_t i = 0; i < tileProviders.size(); i++) {
            bool enabled = i == 0; // Only enable first layer
            std::string name = tileProviders[i].name;
            dest.push_back(properties::BoolProperty(name, name, enabled));
        }
        auto it = dest.begin();
        auto end = dest.end();
        while (it != end) {
            addProperty(*(it++));
        }
    }

    bool RenderableGlobe::initialize() {
        return _distanceSwitch.initialize();
    }

    bool RenderableGlobe::deinitialize() {
        return _distanceSwitch.deinitialize();
    }

    bool RenderableGlobe::isReady() const {
        return _distanceSwitch.isReady();
    }

    void RenderableGlobe::render(const RenderData& data) {
        if (_saveOrThrowCamera.value()) {
            _saveOrThrowCamera.setValue(false);

            if (_chunkedLodGlobe->getSavedCamera() == nullptr) { // save camera
                LDEBUG("Saving snapshot of camera!");
                _chunkedLodGlobe->setSaveCamera(new Camera(data.camera));
            }
            else { // throw camera
                LDEBUG("Throwing away saved camera!");
                _chunkedLodGlobe->setSaveCamera(nullptr);
            }
        }

        _distanceSwitch.render(data);
    }

    void RenderableGlobe::update(const UpdateData& data) {
        _time = data.time;
        _distanceSwitch.update(data);

        _chunkedLodGlobe->doFrustumCulling = doFrustumCulling.value();
        _chunkedLodGlobe->doHorizonCulling = doHorizonCulling.value();
        _chunkedLodGlobe->mergeInvisible = mergeInvisible.value();
        _chunkedLodGlobe->lodScaleFactor = lodScaleFactor.value();
        _chunkedLodGlobe->initChunkVisible = initChunkVisible.value();

        _chunkedLodGlobe->useHeightMap = useHeightMap.value();
        _chunkedLodGlobe->useColorMap = useColorMap.value();
        _chunkedLodGlobe->blendHeightMap = blendHeightMap.value();
        _chunkedLodGlobe->blendColorMap = blendColorMap.value();

        std::vector<TileProviderManager::TileProviderWithName>& colorTextureProviders =
            _tileProviderManager->colorTextureProviders();
        std::vector<TileProviderManager::TileProviderWithName>& heightMapProviders =
            _tileProviderManager->heightMapProviders();

        
        for (size_t i = 0; i < colorTextureProviders.size(); i++) {
            colorTextureProviders[i].isActive = _activeColorLayers[i].value();
        }
        for (size_t i = 0; i < heightMapProviders.size(); i++) {
            heightMapProviders[i].isActive = _activeHeightMapLayers[i].value();
        }
    }

    glm::dvec3 RenderableGlobe::geodeticSurfaceProjection(glm::dvec3 position) {
        return _ellipsoid.geodeticSurfaceProjection(position);
    }


}  // namespace openspace