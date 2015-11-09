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

#ifndef __QUADTREELIST_H__
#define __QUADTREELIST_H__

// std includes
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <fstream>

// ghoul includes
#include <ghoul/opengl/ghoul_gl.h>

namespace openspace {
class QuadtreeList {
public:

    enum class CompressionType : unsigned int {
	NONE = 0,
	LZ4_2D_PRED = 1
    };

    struct CommonMetaData {
        unsigned int nTimesteps;
        unsigned int nBricks;
        CompressionType compressionType;
        unsigned int nBricksPerDim;
        unsigned int brickWidth;
        unsigned int brickHeight;
        unsigned int paddingWidth;
        unsigned int paddingHeight;
        short minEnergy;
        short maxEnergy;
        double minFlux;
        double maxFlux;
        double minExpTime;
        double maxExpTime;
    };

    // Compiler may choose to align values in the struct
    // so that the size becomes larger than
    // the sum of its contained member's sizes.
    static const int _commonMetaDataSize =
      4 * 8 + // Eight 32 bit integer values
      2 * 2 + // Two 16 bit integer values
      8 * 4; // Four 64 bit floating point values

    struct ImageMetaData {
        double exposureTime;
    };

    static const int _imageMetaDataSize = 8; // One 64 bit floating point value

    struct BrickMetaData {
        unsigned int dataPosition;
    };

    static const int _brickMetaDataSize = 4; // One 32 bit integer value

    QuadtreeList(const std::string& filename);
    ~QuadtreeList();

    bool load();
    bool readHeader();

    long long dataPosition(unsigned int brickIndex);

    CompressionType compressionType();
    unsigned int nTimesteps();
    unsigned int nBricksPerDim();
    unsigned int brickWidth();
    unsigned int brickHeight();
    unsigned int paddingWidth();
    unsigned int paddingHeight();
    unsigned int paddedBrickWidth();
    unsigned int paddedBrickHeight();
    unsigned int nQuadtreeLevels();
    unsigned int nQuadtreeNodes();
    unsigned int nTotalNodes();
    double exposureTime(unsigned int brickIndex);
    double minFlux();
    double maxFlux();

    std::ifstream& file();

    unsigned int getFirstChild(unsigned int brickIndex);
    bool isLeaf(unsigned int brickIndex);

private:
    std::string _filename;
    std::ifstream _file;

    // Data from file
    CommonMetaData _commonMetaData;
    ImageMetaData* _imageMetaData;
    BrickMetaData* _brickMetaData;

    // Additional metadata
    unsigned int _paddedBrickWidth;
    unsigned int _paddedBrickHeight;
    unsigned int _nQtLevels;
    unsigned int _nQtNodes;
    unsigned int _nTotalNodes;
    long long _fileSize;

}; // class QuadtreeList

}  // namespace openspace

#endif // __QUADTREELIST_H__
