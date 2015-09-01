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
#include <modules/multiresvolume/rendering/brickselection.h>
#include <modules/multiresvolume/rendering/brickcover.h>


namespace openspace {

class QuadtreeList;
class ImageHistogramManager;
class TransferFunction;

class ImageBrickSelector{
public:
    ImageBrickSelector(QuadtreeList* qtl, ImageHistogramManager* hm, TransferFunction* tf, int brickBudget);
    ~ImageBrickSelector();

    bool initialize();
    void selectBricks(int timestep, std::vector<int>& bricks);
 private:
    QuadtreeList* _quadtreeList;

    int linearCoords(int x, int y, int z);
    void writeSelection(BrickSelection coveredBricks, std::vector<int>& bricks);
};

} // namespace openspace

#endif // __IMAGEBRICKSELECTOR_H__

