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
    , _nTotalNodes(0) {
    _file.open(_filename, std::ios::in | std::ios::binary);
}

QuadtreeList::~QuadtreeList() {
    if (_file.is_open())
        _file.close();
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

    _file.seekg(_file.beg);

    _file.read(reinterpret_cast<char*>(&_header), sizeof(Header));

    LDEBUG("Number of timesteps: " << _header.nTimesteps);
    LDEBUG("Number of bricks: " << _header.nBricksPerDim << " x " << _header.nBricksPerDim);
    LDEBUG("Brick dimensions: " << _header.brickWidth << " x " << _header.brickHeight);

    _paddedBrickWidth = _header.brickWidth + 2 * _header.paddingWidth;
    _paddedBrickHeight = _header.brickHeight + 2 * _header.paddingHeight;

    LDEBUG("Padded brick dimensions: " << _paddedBrickWidth << " x " << _paddedBrickHeight);

    _nQtLevels = static_cast<unsigned int>(log((int)_header.nBricksPerDim) / log(2) + 1);
    _nQtNodes = static_cast<unsigned int>((pow(4, _nQtLevels) - 1) / 3);
    _nTotalNodes = _nQtNodes * _header.nTimesteps;

    LDEBUG("Number of QT levels: " << _nQtLevels);
    LDEBUG("Number of QT nodes: " << _nQtNodes);
    LDEBUG("Total number of nodes: " << _nTotalNodes);

    return true;
}

long long QuadtreeList::dataPosition() {
    return sizeof(Header);
}

unsigned int QuadtreeList::nTimesteps() {
    return _header.nTimesteps;
}

unsigned int QuadtreeList::nBricksPerDim() {
    return _header.nBricksPerDim;
}

unsigned int QuadtreeList::brickWidth() {
    return _header.brickWidth;
}

unsigned int QuadtreeList::brickHeight() {
    return _header.brickHeight;
}

unsigned int QuadtreeList::paddingWidth() {
    return _header.paddingWidth;
}

unsigned int QuadtreeList::paddingHeight() {
    return _header.paddingHeight;
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
