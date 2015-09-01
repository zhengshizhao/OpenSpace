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

#ifndef __RENDERABLEMULTIRESPLANE_H__
#define __RENDERABLEMULTIRESPLANE_H__

#include <openspace/rendering/renderable.h>
#include <openspace/properties/vectorproperty.h>
#include <ghoul/misc/dictionary.h>

// Forward declare to minimize dependencies
namespace ghoul {
    namespace filesystem {
        class File;
    }
    namespace opengl {
        class ProgramObject;
        class Texture;
    }
}

namespace openspace {
// Forward declare
class QuadtreeList;
class ImageAtlasManager;
class ImageBrickSelector;

class RenderableMultiresPlane: public Renderable {
public:
    RenderableMultiresPlane(const ghoul::Dictionary& dictionary);
    ~RenderableMultiresPlane();

    bool initialize() override;
    bool deinitialize() override;
    bool isReady() const override;
    void render(const RenderData& data) override;

private:
    void createPlane();

    int _timestep;
    int _brickBudget;
    properties::Vec2Property _size;

    std::string _filename;
    ghoul::opengl::ProgramObject* _shader;

    QuadtreeList* _quadtreeList;
    ImageAtlasManager* _atlasManager;
    ImageBrickSelector* _brickSelector;

    std::vector<int> _brickIndices;

    GLuint _quad;
    GLuint _vertexPositionBuffer;
};

} // namespace openspace

#endif // __RENDERABLEMULTIRESPLANE_H__
