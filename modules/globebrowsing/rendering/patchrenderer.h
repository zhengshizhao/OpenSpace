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

#ifndef __LATLONPATCH_H__
#define __LATLONPATCH_H__

#include <memory>
#include <glm/glm.hpp>

// open space includes
#include <openspace/rendering/renderable.h>

#include <modules/globebrowsing/geodetics/geodetic2.h>
#include <modules/globebrowsing/geodetics/ellipsoid.h>
#include <modules/globebrowsing/meshes/grid.h>
//#include <modules/globebrowsing/rendering/frustumculler.h>
#include <modules/globebrowsing/other/tileprovidermanager.h>

#include <modules/globebrowsing/other/layeredtextureshaderprovider.h>
#include <modules/globebrowsing/globes/chunknode.h>

namespace ghoul {
namespace opengl {
    class ProgramObject;
}
}


namespace openspace {

    class TriangleSoup;
    
    using std::shared_ptr;
    using std::unique_ptr;
    using ghoul::opengl::ProgramObject;

    class PatchRenderer {
    public:
        
        PatchRenderer(std::shared_ptr<TileProviderManager> _tileProviderManager);
        ~PatchRenderer();

        void update();

    protected:
        std::shared_ptr<TileProviderManager> _tileProviderManager;
    };


    //////////////////////////////////////////////////////////////////////////////////////
    //							PATCH RENDERER SUBCLASSES								//
    //////////////////////////////////////////////////////////////////////////////////////

    class ChunkRenderer : public PatchRenderer {
    public:
        ChunkRenderer(shared_ptr<Grid> grid, 
                      shared_ptr<TileProviderManager> tileProviderManager);

        void renderChunk(const Chunk& chunk, const RenderData& data);

    private:
        void renderChunkGlobally(
            const Chunk& chunk, const RenderData& data);
        void renderChunkLocally(
            const Chunk& chunk, const RenderData& data);

      shared_ptr<Grid> _grid;
        unique_ptr<LayeredTextureShaderProvider> _globalRenderingShaderProvider;
        unique_ptr<LayeredTextureShaderProvider> _localRenderingShaderProvider;
    };






}  // namespace openspace

#endif  // __LATLONPATCH_H__