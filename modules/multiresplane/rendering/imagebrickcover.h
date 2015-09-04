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

#ifndef __IMAGEBRICKCOVER_H__
#define __IMAGEBRICKCOVER_H__

namespace openspace {

struct ImageBrickCover {
    int lowX, highX, lowY, highY;

    ImageBrickCover() {}
    ImageBrickCover(int numBricks) {
        lowX = lowY = 0;
        highX = highY = numBricks;
    }

    ImageBrickCover split(bool x, bool y) {
        ImageBrickCover child;
        if (x) {
            child.lowX = lowX + (highX - lowX) / 2;
            child.highX = highX;
        } else {
            child.lowX = lowX;
            child.highX = lowX + (highX - lowX) / 2;
        }
        if (y) {
            child.lowY = lowY + (highY - lowY) / 2;
            child.highY = highY;
        } else {
            child.lowY = lowY;
            child.highY = lowY + (highY - lowY) / 2;
        }
        return child;
    }
};
}

#endif // __IMAGEBRICKCOVER_H__
