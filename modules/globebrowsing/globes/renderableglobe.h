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

#ifndef __RENDERABLEGLOBE_H__
#define __RENDERABLEGLOBE_H__

#include <ghoul/logging/logmanager.h>

// open space includes
#include <openspace/rendering/renderable.h>

#include <openspace/properties/stringproperty.h>
#include <openspace/properties/optionproperty.h>
#include <openspace/util/updatestructures.h>

#include <modules/globebrowsing/meshes/trianglesoup.h>
#include <modules/globebrowsing/other/distanceswitch.h>
#include <modules/globebrowsing/globes/globemesh.h>
#include <modules/globebrowsing/globes/chunkedlodglobe.h>

#include <modules/globebrowsing/geodetics/ellipsoid.h>
#include <modules/globebrowsing/other/tileprovidermanager.h>
#include <modules/globebrowsing/other/threadpool.h>

namespace ghoul {
namespace opengl {
    class ProgramObject;
}
}


namespace openspace {

class RenderableGlobe : public Renderable {
public:
    RenderableGlobe(const ghoul::Dictionary& dictionary);
    ~RenderableGlobe();

    bool initialize() override;
    bool deinitialize() override;
    bool isReady() const override;

    void render(const RenderData& data) override;
    void update(const UpdateData& data) override;

    glm::dvec3 geodeticSurfaceProjection(glm::dvec3 position);

    properties::BoolProperty doFrustumCulling;
    properties::BoolProperty doHorizonCulling;
    properties::BoolProperty mergeInvisible;
    properties::FloatProperty lodScaleFactor;
    properties::BoolProperty initChunkVisible;
    properties::BoolProperty renderSmallChunksFirst;

    // Layered rendering
    properties::BoolProperty useHeightMap;
    properties::BoolProperty useColorMap;
    properties::BoolProperty blendHeightMap;
    properties::BoolProperty blendColorMap;

private:

    void addToggleLayerProperties(
        std::vector<TileProviderManager::TileProviderWithName>&,
        std::vector<properties::BoolProperty>& dest
    );

    double _time;

    Ellipsoid _ellipsoid;

    //std::vector<std::string> _heightMapKeys;
    //std::vector<std::string> _colorTextureKeys;


    std::shared_ptr<TileProviderManager> _tileProviderManager;
    std::shared_ptr<ChunkedLodGlobe> _chunkedLodGlobe;
    
    properties::BoolProperty _saveOrThrowCamera;

    std::vector<properties::BoolProperty> _activeColorLayers;
    std::vector<properties::BoolProperty> _activeHeightMapLayers;

    DistanceSwitch _distanceSwitch;
};

}  // namespace openspace

#endif  // __RENDERABLEGLOBE_H__