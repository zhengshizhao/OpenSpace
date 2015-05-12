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

#ifndef __BRICKSELECTOR_H__
#define __BRICKSELECTOR_H__

#include <vector>
#include <openspace/rendering/flare/tsp.h>

namespace openspace {

class BrickSelector {
public:
    BrickSelector(TSP* tsp, float spatialTolerance, float temporalTolerance);
    ~BrickSelector();

    void selectBricks(int timestep,
                      int* bricks);
private:
    TSP* _tsp;
    float _spatialTolerance;
    float _temporalTolerance;

    void traverseOT(int timestep,
                    unsigned int brickIndex,
                    int* bricks);

    void traverseBST(int timestep,
                     unsigned int brickIndex,
                     unsigned int bstRootBrickIndex,
                     int timeSpanStart,
                     int timeSpanEnd,
                     int* bricks);

    void selectBricks(int timestep,
                      unsigned int brickIndex,
                      unsigned int bstRootBrickIndex,
                      int timeSpanStart,
                      int timeSpanEnd,
                      int* bricks);
};

} // namespace openspace

#endif // __BRICKSELECTOR_H__