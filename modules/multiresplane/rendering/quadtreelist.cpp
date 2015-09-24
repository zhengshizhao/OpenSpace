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

#include <sgct.h>
#include <modules/multiresplane/rendering/quadtreelist.h>

// ghoul
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/logging/logmanager.h>

// std
#include <algorithm>
#include <queue>

namespace {
    const std::string _loggerCat = "QuadtreeList";
}

namespace openspace {

QuadtreeList::QuadtreeList(const std::string& filename)
    : _filename(filename)
    , _paddedBrickWidth(0)
    , _paddedBrickHeight(0)
    , _nQtLevels(0)
    , _nQtNodes(0)
    , _nTotalNodes(0)
    , _fileSize(0)
    , _imageMetaData(nullptr) {
    _file.open(_filename, std::ios::in | std::ios::binary);
}

QuadtreeList::~QuadtreeList() {
    if (_file.is_open())
        _file.close();
    if (_imageMetaData) {
	delete _imageMetaData;
    }
    if (_brickMetaData) {
	delete _brickMetaData;
    }
}

bool QuadtreeList::load() {
    if (!readHeader()) {
        LERROR("Could not read header");
        return false;
    }
    return true;
}

bool QuadtreeList::readHeader() {
    if (!_file.good())
        return false;

    _file.seekg(0, _file.beg);
    char commonMeta[_commonMetaDataSize];

    _file.read(reinterpret_cast<char*>(commonMeta), _commonMetaDataSize);
    char* ptr = commonMeta;
    _commonMetaData.nTimesteps = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.nBricks = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.compressionType = *reinterpret_cast<QuadtreeList::CompressionType*>(ptr);
    ptr += sizeof(QuadtreeList::CompressionType);
    _commonMetaData.nBricksPerDim = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.brickWidth = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.brickHeight = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.paddingWidth = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.paddingHeight = *reinterpret_cast<unsigned int*>(ptr);
    ptr += sizeof(unsigned int);
    _commonMetaData.minEnergy = *reinterpret_cast<int16_t*>(ptr);
    ptr += sizeof(int16_t);
    _commonMetaData.maxEnergy = *reinterpret_cast<int16_t*>(ptr);
    ptr += sizeof(int16_t);
    _commonMetaData.minFlux = *reinterpret_cast<double*>(ptr);
    ptr += sizeof(double);
    _commonMetaData.maxFlux = *reinterpret_cast<double*>(ptr);
    ptr += sizeof(double);
    _commonMetaData.minExpTime = *reinterpret_cast<double*>(ptr);
    ptr += sizeof(double);
    _commonMetaData.maxExpTime = *reinterpret_cast<double*>(ptr);
    ptr += sizeof(double);


    LDEBUG("Number of timesteps: " << _commonMetaData.nTimesteps);
    LDEBUG("Number of bricks: " << _commonMetaData.nBricksPerDim << " x " << _commonMetaData.nBricksPerDim);
    LDEBUG("Compression type: " << static_cast<unsigned int>(_commonMetaData.compressionType));

    LDEBUG("Brick dimensions: " << _commonMetaData.brickWidth << " x " << _commonMetaData.brickHeight);

    _paddedBrickWidth = _commonMetaData.brickWidth + 2 * _commonMetaData.paddingWidth;
    _paddedBrickHeight = _commonMetaData.brickHeight + 2 * _commonMetaData.paddingHeight;

    LDEBUG("Padded brick dimensions: " << _paddedBrickWidth << " x " << _paddedBrickHeight);

    _nQtLevels = static_cast<unsigned int>(log((int)_commonMetaData.nBricksPerDim) / log(2) + 1);
    _nQtNodes = static_cast<unsigned int>((pow(4, _nQtLevels) - 1) / 3);
    _nTotalNodes = _nQtNodes * _commonMetaData.nTimesteps;

    LDEBUG("Number of QT levels: " << _nQtLevels);
    LDEBUG("Number of QT nodes: " << _nQtNodes);
    LDEBUG("Total number of nodes: " << _nTotalNodes);

    unsigned int nTimesteps = _commonMetaData.nTimesteps;
    char* imageMeta = new char[_imageMetaDataSize * nTimesteps];
    _imageMetaData = new ImageMetaData[nTimesteps];
    _file.read(imageMeta, _imageMetaDataSize*nTimesteps);

    for (int i = 0; i < nTimesteps; ++i) {
	unsigned int offset = i*_imageMetaDataSize;
	_imageMetaData[i].exposureTime = *reinterpret_cast<double*>(imageMeta + offset);
    }
    delete[] imageMeta;

    unsigned int nBricks = _commonMetaData.nBricks;
    char* brickMeta = new char[_brickMetaDataSize * nBricks];
    _brickMetaData = new BrickMetaData[nBricks];
    _file.read(brickMeta, _brickMetaDataSize*nBricks);

    for (int i = 0; i < nBricks; ++i) {
	unsigned int offset = i*_brickMetaDataSize;
	_brickMetaData[i].dataPosition = *reinterpret_cast<unsigned int*>(brickMeta + offset);
    }
    delete[] brickMeta;

    _file.seekg(0, _file.end);
    _fileSize = _file.tellg();

    return true;
}

long long QuadtreeList::dataPosition(unsigned int brickIndex) {
    unsigned int nBricks = nTotalNodes();
    if (brickIndex < nBricks) {
	return _brickMetaData[brickIndex].dataPosition;
    } else if (brickIndex == nBricks) {
	return _fileSize;
    } else {
	LERROR("Trying to get brick data position for index " << brickIndex << " which is outside bounds.");
    }
}

unsigned int QuadtreeList::nTimesteps() {
    return _commonMetaData.nTimesteps;
}

QuadtreeList::CompressionType QuadtreeList::compressionType() {
    return _commonMetaData.compressionType;
}

unsigned int QuadtreeList::nBricksPerDim() {
    return _commonMetaData.nBricksPerDim;
}

unsigned int QuadtreeList::brickWidth() {
    return _commonMetaData.brickWidth;
}

unsigned int QuadtreeList::brickHeight() {
    return _commonMetaData.brickHeight;
}

unsigned int QuadtreeList::paddingWidth() {
    return _commonMetaData.paddingWidth;
}

unsigned int QuadtreeList::paddingHeight() {
    return _commonMetaData.paddingHeight;
}

unsigned int QuadtreeList::paddedBrickWidth() {
    return _paddedBrickWidth;
}

unsigned int QuadtreeList::paddedBrickHeight() {
    return _paddedBrickHeight;
}

unsigned int QuadtreeList::nQuadtreeLevels() {
    return _nQtLevels;
}

unsigned int QuadtreeList::nQuadtreeNodes() {
    return _nQtNodes;
}

unsigned int QuadtreeList::nTotalNodes() {
    return _nTotalNodes;
}

std::ifstream& QuadtreeList::file() {
	return _file;
}

double QuadtreeList::minFlux() {
    return _commonMetaData.minFlux;
}

double QuadtreeList::maxFlux() {
    return _commonMetaData.maxFlux;
}

double QuadtreeList::exposureTime(unsigned int brickIndex) {
    return _imageMetaData[brickIndex/_nQtNodes].exposureTime;
}

unsigned int QuadtreeList::getFirstChild(unsigned int brickIndex) {
    int qtNode = brickIndex % _nQtNodes;
    unsigned int timestepOffset = brickIndex - qtNode;
    int depth = log(3 * qtNode + 1) / log(4);
    int firstInLevel = (pow(4, depth) - 1) / 3;
    int levelOffset = qtNode - firstInLevel;
    unsigned int firstInChildLevel = (pow(4, depth + 1) - 1) / 3;
    unsigned int childIndex = firstInChildLevel + 4*levelOffset;
    return timestepOffset + childIndex;
}

bool QuadtreeList::isLeaf(unsigned int brickIndex) {
    unsigned int qtNode = brickIndex % _nQtNodes;
    int depth = floor(log(3 * qtNode + 1) / log(4));
    return depth == _nQtLevels - 1;
}

}
