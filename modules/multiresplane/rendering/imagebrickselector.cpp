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

#include <modules/multiresplane/rendering/imagebrickselector.h>

#include <modules/multiresplane/rendering/quadtreelist.h>

#include <ghoul/logging/logmanager.h>

#include <sgct.h>
#include <map>
#include <algorithm>
#include <queue>

namespace {
    const std::string _loggerCat = "ImageBrickSelector";
}

namespace openspace {

ImageBrickSelector::ImageBrickSelector(QuadtreeList* qtl, std::vector<glm::vec4> quadCorners)
    : _prevResolution(0,0)
    , _prevUsedBricks(0) {
    _quadtreeList = qtl;
    for (auto& corner : quadCorners) {
        _quadCorners.push_back(psc(corner));
    }
}

ImageBrickSelector::~ImageBrickSelector() {}

bool ImageBrickSelector::initialize() {
    return true;
}

void ImageBrickSelector::selectBricks(int timestep, const RenderData& renderData, std::vector<int>& bricks) {
    // Set largest acceptable voxel size
    float acceptableVoxelSize = 1.0;

    // Update screen resolution
    int width = sgct::Engine::instance()->getActiveWindowPtr()->getXFramebufferResolution();
    int height = sgct::Engine::instance()->getActiveWindowPtr()->getYFramebufferResolution();
    _screenResolution = glm::ivec2(width, height);

    int nQtLevels = _quadtreeList->nQuadtreeLevels();
    int nBricksPerDim = _quadtreeList->nBricksPerDim();
    int nQuadtreeNodes = _quadtreeList->nQuadtreeNodes();

    // Get quadtree root of timestep
    unsigned int qtRootIndex = nQuadtreeNodes * timestep;
    ImageBrickCover qtRootCover = ImageBrickCover(nBricksPerDim);

    std::queue< std::pair<unsigned int, ImageBrickCover> > brickQueue;
    brickQueue.push(std::pair<unsigned int, ImageBrickCover>(qtRootIndex, qtRootCover));

    int nUsedBricks = 0;
    int nInvisible = 0;
    while (brickQueue.size() > 0) {
        unsigned int brickIndex = brickQueue.front().first;
        ImageBrickCover brickCover = brickQueue.front().second;
        brickQueue.pop();
        if (isVisible(brickCover, renderData)) {
            if (!_quadtreeList->isLeaf(brickIndex)) {
                float voxelSize = voxelSizeInScreenSpace(brickCover, renderData);
                if (voxelSize > acceptableVoxelSize) {
                    // Resolution not acceptable, split
                    unsigned int firstChild = _quadtreeList->getFirstChild(brickIndex);
                    for (int c = 0; c < 4; c++) {
                        unsigned int childIndex = firstChild + c;
                        ImageBrickCover childCover = brickCover.split(c % 2, c / 2);
                        brickQueue.push(std::pair<unsigned int, ImageBrickCover>(childIndex, childCover));
                    }
                } else {
                    // Resolution is acceptable, select brick
                    writeSelection(brickIndex, brickCover, bricks);
                    nUsedBricks++;
                }
            } else {
                // Brick is leaf, select brick
                writeSelection(brickIndex, brickCover, bricks);
                nUsedBricks++;
            }
        } else {
            // Brick not visible, set cover to -1
            writeSelection(-1, brickCover, bricks);
            nInvisible++;
        }
    }

    if (nUsedBricks != _prevUsedBricks) {
        LINFO("used bricks: " << nUsedBricks);
        _prevUsedBricks = nUsedBricks;
    }
}

bool ImageBrickSelector::isVisible(ImageBrickCover brickCover, const RenderData& renderData) {
    int nBricksPerDim = _quadtreeList->nBricksPerDim();
    float s0 = float(brickCover.lowX) / nBricksPerDim;
    float t0 = float(brickCover.lowY) / nBricksPerDim;
    float s1 = float(brickCover.highX) / nBricksPerDim;
    float t1 = float(brickCover.highY) / nBricksPerDim;

    psc v1 = _quadCorners[1] - _quadCorners[0];
    psc v2 = _quadCorners[2] - _quadCorners[0];

    psc c0 = _quadCorners[0] + v1*s0 + v2*t0;
    psc c1 = _quadCorners[0] + v1*s1 + v2*t0;
    psc c2 = _quadCorners[0] + v1*s0 + v2*t1;
    psc c3 = _quadCorners[0] + v1*s1 + v2*t1;

    // Screen space axis aligned bounding box
    glm::vec4 ssaabb = screenSpaceBoundingBox(c0, c1, c2, c3, renderData);

    // Calculate sides of intersection
    float left = std::max(std::min(ssaabb.x, ssaabb.z), -1.0f);
    float right = std::min(std::max(ssaabb.x, ssaabb.z), 1.0f);

    float top = std::max(std::min(ssaabb.y, ssaabb.w), -1.0f);
    float bottom = std::min(std::max(ssaabb.y, ssaabb.w), 1.0f);

    // ssaabb is visible if intersection has positive area
    return left < right && top < bottom;
}

float ImageBrickSelector::voxelSizeInScreenSpace(ImageBrickCover brickCover, const RenderData& renderData) {
    // Largest coverage of the screen in one dimension

    int nBricksPerDim = _quadtreeList->nBricksPerDim();
    float s0 = float(brickCover.lowX) / nBricksPerDim;
    float t0 = float(brickCover.lowY) / nBricksPerDim;
    float s1 = float(brickCover.highX) / nBricksPerDim;
    float t1 = float(brickCover.highY) / nBricksPerDim;

    psc v1 = _quadCorners[1] - _quadCorners[0];
    psc v2 = _quadCorners[2] - _quadCorners[0];

    psc c0 = _quadCorners[0] + v1*s0 + v2*t0;
    psc c1 = _quadCorners[0] + v1*s1 + v2*t0;
    psc c2 = _quadCorners[0] + v1*s0 + v2*t1;

    glm::vec2 brickSize = quadSizeInScreenSpace(c0, c1, c2, renderData);

    float pixelsCoveredX = brickSize.x * _screenResolution.x;
    float pixelsCoveredY = brickSize.y * _screenResolution.y;

    glm::vec2 centerVoxelSize = glm::vec2(
                                    pixelsCoveredX / _quadtreeList->brickWidth(),
                                    pixelsCoveredY / _quadtreeList->brickHeight()
                                );

    return std::max(centerVoxelSize.x, centerVoxelSize.y);
}

glm::vec2 ImageBrickSelector::quadSizeInScreenSpace(psc c0, psc c1, psc c2, const RenderData& renderData) {
    // Largest coverage of the screen in one dimension
    psc c3 = c1 + c2 - c0;

    glm::vec4 ssaabb = screenSpaceBoundingBox(c0, c1, c2, c3, renderData);
    float xCoverage = (ssaabb.z - ssaabb.x) / 2;
    float yCoverage = (ssaabb.w - ssaabb.y) / 2;

    return glm::vec2(xCoverage, yCoverage);
}

glm::vec4 ImageBrickSelector::screenSpaceBoundingBox(psc c0, psc c1, psc c2, psc c3, const RenderData& renderData) {
    // Axis aligned bounding box in screen space
    // [minX, minY, maxX, maxY];
    glm::vec2 c0screen = modelToScreenSpace(c0, renderData);
    glm::vec2 c1screen = modelToScreenSpace(c1, renderData);
    glm::vec2 c2screen = modelToScreenSpace(c2, renderData);
    glm::vec2 c3screen = modelToScreenSpace(c3, renderData);

    float minX = std::min(c0screen.x, std::min(c1screen.x, std::min(c2screen.x, c3screen.x)));
    float minY = std::min(c0screen.y, std::min(c1screen.y, std::min(c2screen.y, c3screen.y)));
    float maxX = std::max(c0screen.x, std::max(c1screen.x, std::max(c2screen.x, c3screen.x)));
    float maxY = std::max(c0screen.y, std::max(c1screen.y, std::max(c2screen.y, c3screen.y)));

    return glm::vec4(minX, minY, maxX, maxY);
}

glm::vec2 ImageBrickSelector::modelToScreenSpace(psc point, const RenderData& renderData) {
    // Do the same as pscTransform in powerScaling_vs.hglsl:
    // vec4 position = pscTransform(worldPosition, modelTransform);

    // vec3 local_vertex_pos = mat3(modelTransform) * vertexPosition.xyz;
    glm::vec3 local_vertex_pos = glm::mat3(renderData.camera.modelMatrix()) * point.vec4().xyz();

    // vertexPosition = psc_addition(vec4(local_vertex_pos,vertexPosition.w),objpos);
    psc vertexPosition = psc(glm::vec4(local_vertex_pos, point.vec4().w)) + renderData.position;

    // vertexPosition = psc_addition(vertexPosition,vec4(-campos.xyz,campos.w));
    vertexPosition -= renderData.camera.position();

    // vertexPosition.xyz =  mat3(camrot) * vertexPosition.xyz;
    glm::mat3 camRot = glm::mat3(renderData.camera.viewRotationMatrix());
    glm::vec4 vpVector = vertexPosition.vec4();
    vpVector = glm::vec4(camRot * vpVector.xyz(), vpVector.w);

    // vec4 tmp = vertexPosition;
    // tmp = psc_to_meter(tmp, scaling);
    psc tmp = psc(vpVector);
    PowerScaledScalar scaling = PowerScaledScalar(renderData.camera.scaling());
    tmp *= scaling;
    glm::vec4 position = glm::vec4(tmp.vec3(), 1.0);
    // position is returned from pscTransform

    // Do the same as nearPlaneProjection in in powerScaling_vs.hglsl
    // gl_Position =  nearPlaneProjection(viewProjection * position);

    // We only need x and y of the result, so let's keep it simple
    glm::vec4 v_in = renderData.camera.viewProjectionMatrix() * position;

    // However, we also need to do the perspective divide
    if (v_in.w != 0.0) {
        v_in.xy = v_in.xy() / std::abs(v_in.w);
    }

    return v_in.xy();
}

int ImageBrickSelector::linearCoords(int x, int y) {
    return x + (_quadtreeList->nBricksPerDim() * y);
}

void ImageBrickSelector::writeSelection(int brickIndex, ImageBrickCover brickCover, std::vector<int>& bricks) {
    for (int y = brickCover.lowY; y < brickCover.highY; y++) {
        for (int x = brickCover.lowX; x < brickCover.highX; x++) {
            bricks[linearCoords(x, y)] = brickIndex;
        }
    }
}


} // namespace openspace
