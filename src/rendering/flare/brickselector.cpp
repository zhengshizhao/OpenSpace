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

#include <openspace/rendering/flare/brickselector.h>

namespace {
    const std::string _loggerCat = "BrickSelector";
}

namespace openspace {

BrickSelector::BrickSelector(TSP* tsp, float spatialTolerance, float temporalTolerance)
    : _tsp(tsp)
    , _spatialTolerance(spatialTolerance)
    , _temporalTolerance(temporalTolerance) {}

BrickSelector::~BrickSelector() {

}

void BrickSelector::selectBricks(int timestep, int* bricks) {
    int numTimeSteps = _tsp->header().numTimesteps_;
    selectBricks(timestep, 0, 0, 0, numTimeSteps, bricks);
}

/**
 * Traverse the Octree in the BST root
 */
void BrickSelector::traverseOT(int timestep, unsigned int brickIndex, int* bricks) {
    unsigned int firstChild = _tsp->getFirstChild(brickIndex);
    int numTimeSteps = _tsp->header().numTimesteps_;
    for (unsigned int child = firstChild; child < firstChild + 8; child++) {
        selectBricks(timestep, child, child, 0, numTimeSteps, bricks);
    }
}

void BrickSelector::traverseBST(int timestep,
                                unsigned int brickIndex,
                                unsigned int bstRootBrickIndex,
                                int timeSpanStart,
                                int timeSpanEnd,
                                int* bricks) {


    int timeSpanCenter = timeSpanStart + (timeSpanEnd - timeSpanStart) / 2;
    unsigned int bstChild;
    if (timestep <= timeSpanCenter) {
        bstChild = _tsp->getBstLeft(brickIndex);
        timeSpanEnd = timeSpanCenter;
    } else {
        bstChild = _tsp->getBstRight(brickIndex);
        timeSpanStart = timeSpanCenter;
    }
    selectBricks(timestep, bstChild, bstRootBrickIndex, timeSpanStart, timeSpanEnd, bricks);
}

void BrickSelector::selectBricks(int timestep,
                                unsigned int brickIndex,
                                unsigned int bstRootBrickIndex,
                                int timeSpanStart,
                                int timeSpanEnd,
                                int* bricks) {

    if (_tsp->getTemporalError(brickIndex) <= _temporalTolerance) {
        if (_tsp->isOctreeLeaf(bstRootBrickIndex)) {
            bricks[brickIndex] = 1;
        } else if (_tsp->getSpatialError(brickIndex) <= _spatialTolerance) {
            bricks[brickIndex] = 1;
        } else if (_tsp->isBstLeaf(brickIndex)) {
            traverseOT(timestep, bstRootBrickIndex, bricks);
        } else {
            traverseBST(timestep, brickIndex, bstRootBrickIndex, timeSpanStart, timeSpanEnd, bricks);
        }
    } else if (_tsp->isBstLeaf(brickIndex)) {
        if (_tsp->isOctreeLeaf(bstRootBrickIndex)) {
            bricks[brickIndex] = 1;
        } else {
            traverseOT(timestep, bstRootBrickIndex, bricks);
        }
    } else {
        traverseBST(timestep, brickIndex, bstRootBrickIndex, timeSpanStart, timeSpanEnd, bricks);
    }
}

} // namespace openspace