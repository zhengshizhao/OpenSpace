/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2015                                                                    *
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

#ifndef __IMAGEBRICKSELECTOR_H__
#define __IMAGEBRICKSELECTOR_H__

#include <vector>

#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/util/powerscaledscalar.h>
#include <modules/multiresplane/rendering/imagebrickcover.h>

#include <openspace/util/updatestructures.h>

namespace openspace {

class QuadtreeList;
class TransferFunction;

class ImageBrickSelector{
public:
    ImageBrickSelector(QuadtreeList* qtl, std::vector<glm::vec4> quadCorners);
    ~ImageBrickSelector();

    bool initialize();
    void selectBricks(int timestep, const RenderData& renderData, std::vector<int>& bricks);
 private:
    QuadtreeList* _quadtreeList;
    std::vector<psc> _quadCorners;
    glm::ivec2 _screenResolution;

    bool isVisible(ImageBrickCover brickCover, const RenderData& renderData);
    float voxelSizeInScreenSpace(ImageBrickCover brickCover, const RenderData& renderData);
    glm::vec2 quadSizeInScreenSpace(psc c0, psc c1, psc c2, const RenderData& renderData);
    glm::vec4 screenSpaceBoundingBox(psc c0, psc c1, psc c2, psc c3, const RenderData& renderData);
    glm::vec2 modelToScreenSpace(psc point, const RenderData& renderData);
    int linearCoords(int x, int y);
    void writeSelection(int brickIndex, ImageBrickCover brickCover, std::vector<int>& bricks);

    // DEBUG
    int _prevUsedBricks;
    glm::ivec2 _prevResolution;
};

} // namespace openspace

#endif // __IMAGEBRICKSELECTOR_H__

