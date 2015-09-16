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

#include <modules/multiresplane/rendering/quadtreelist.h>
#include <modules/multiresplane/rendering/imageatlasmanager.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/logging/logmanager.h>
#include <string>
#include <cstring>

namespace {
    const std::string _loggerCat = "ImageAtlasManager";
}

namespace openspace {

ImageAtlasManager::ImageAtlasManager(QuadtreeList* qtl, unsigned int atlasCapacity) 
    : _quadtreeList(qtl)
    , _atlasCapacity(atlasCapacity)
    , _needsReinitialization(true)
    , _pboHandle(0)
    , _atlasMapBuffer(0) {}

ImageAtlasManager::~ImageAtlasManager() {}

void ImageAtlasManager::setAtlasCapacity(unsigned int atlasCapacity) {
    _atlasCapacity = atlasCapacity;
    _needsReinitialization = true;
}

bool ImageAtlasManager::initialize() {
    if (_textureAtlas) {
	delete _textureAtlas;
    }
    if (_pboHandle) {
	glDeleteBuffers(1, &_pboHandle);
    }
    if (_atlasMapBuffer) {
	glDeleteBuffers(1, &_atlasMapBuffer);
    }

    _nBricksPerDim = _quadtreeList->nBricksPerDim();
    unsigned int paddedBrickWidth = _quadtreeList->paddedBrickWidth();
    unsigned int paddedBrickHeight = _quadtreeList->paddedBrickHeight();
    _paddedBrickDims = glm::ivec2(paddedBrickWidth, paddedBrickHeight);
    _nBrickVals = paddedBrickWidth*paddedBrickHeight;
    _brickSize = _nBrickVals*sizeof(GLshort);

    _atlasBricksPerDim = std::ceil(std::sqrt(_atlasCapacity));
    _atlasWidth = _atlasBricksPerDim * paddedBrickWidth;
    _atlasHeight = _atlasBricksPerDim * paddedBrickHeight;
    _atlasSize = _atlasWidth * _atlasHeight * sizeof(GLfloat);


    _nQtLeaves = _nBricksPerDim * _nBricksPerDim;
    _nQtLevels = _quadtreeList->nQuadtreeLevels();
    _nQtNodes = _quadtreeList->nQuadtreeNodes();
    _textureAtlas = new ghoul::opengl::Texture(glm::size3_t(_atlasWidth, _atlasHeight, 1), 
					       ghoul::opengl::Texture::Format::RGB,
					       GL_RGBA,
					       GL_FLOAT);
    _textureAtlas->uploadTexture();

    glGenBuffers(1, &_pboHandle);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pboHandle);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, _atlasSize, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glGenBuffers(1, &_atlasMapBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _atlasMapBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLint)*_nQtLeaves, NULL, GL_DYNAMIC_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);



    _atlasMap = std::vector<unsigned int>(_nQtLeaves, NOT_USED);

    _freeAtlasCoords = std::vector<unsigned int>(_atlasCapacity, 0);
    for (unsigned int i = 0; i < _atlasCapacity; i++) {
        _freeAtlasCoords[i] = i;
    }
    
    _needsReinitialization = false;
	return true;
}

void ImageAtlasManager::updateAtlas(std::vector<int>& brickIndices) {

    if (_needsReinitialization) {
	initialize();
    }
    
    int nBrickIndices = brickIndices.size();

    _requiredBricks.clear();
    for (int i = 0; i < nBrickIndices; i++) {
	int requiredIndex = brickIndices[i];
	if (requiredIndex >= 0) {
	    _requiredBricks.insert(requiredIndex);
	}
    }

    // for now: remove all previously required bricks
    // possibly put them in a queue for possible removal
    // (keeping things in cache as long as possible)
    for (unsigned int it : _prevRequiredBricks) {
	if (!_requiredBricks.count(it)) {
	    removeFromAtlas(it);
	}
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pboHandle);
    GLfloat* mappedBuffer = reinterpret_cast<GLfloat*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
    
    if (!mappedBuffer) {
	LERROR("Failed to map PBO.");
	LERROR(glGetError());
	assert(false);
	return;
    }
    
    for (auto itStart = _requiredBricks.begin(); itStart != _requiredBricks.end();) {
	int firstBrick = *itStart;
	int lastBrick = firstBrick;
	
	auto itEnd = itStart;
        for (itEnd++; itEnd != _requiredBricks.end() && *itEnd == lastBrick + 1; itEnd++) {
            lastBrick = *itEnd;
        }

        addToAtlas(firstBrick, lastBrick, mappedBuffer);
	
        itStart = itEnd;
    }

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    for (int i = 0; i < nBrickIndices; i++) {
	int brickIndex = brickIndices[i];
	if (brickIndex >= 0) {
	    _atlasMap[i] = _brickMap[brickIndex];
	} else {
	    _atlasMap[i] = 0;
	}
    }

    std::swap(_prevRequiredBricks, _requiredBricks);

    pboToAtlas();

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _atlasMapBuffer);
    GLint *to = (GLint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    if (!to) {
	LERROR("Failed to map SSBO.");
	LERROR(glGetError());
	assert(false);
	return;
    }
    memcpy(to, _atlasMap.data(), sizeof(GLint)*_atlasMap.size());
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ImageAtlasManager::addToAtlas(int firstBrickIndex, int lastBrickIndex, GLfloat* mappedBuffer) {
    while (_brickMap.count(firstBrickIndex) && firstBrickIndex <= lastBrickIndex) firstBrickIndex++;
    while (_brickMap.count(lastBrickIndex) && lastBrickIndex >= firstBrickIndex) lastBrickIndex--;
    if (lastBrickIndex < firstBrickIndex) return;

    int sequenceLength = lastBrickIndex - firstBrickIndex + 1;
    GLshort* sequenceBuffer = new GLshort[sequenceLength*_nBrickVals];
    size_t bufferSize = sequenceLength * _brickSize;

    long offset = _quadtreeList->dataPosition() + static_cast<long>(firstBrickIndex) * static_cast<long>(_brickSize);
    _quadtreeList->file().seekg(offset);
    _quadtreeList->file().read(reinterpret_cast<char*>(sequenceBuffer), bufferSize);

    for (int brickIndex = firstBrickIndex; brickIndex <= lastBrickIndex; brickIndex++) {
        if (!_brickMap.count(brickIndex)) {
            unsigned int atlasCoords = _freeAtlasCoords.back();
            _freeAtlasCoords.pop_back();
            int level = _nQtLevels - floor(log((3.0 * (float(brickIndex % _nQtNodes)) + 1.0))/log(4)) - 1;
            assert(atlasCoords <= 0x0FFFFFFF);
            unsigned int atlasData = (level << 28) + atlasCoords;
            _brickMap.insert(std::pair<unsigned int, unsigned int>(brickIndex, atlasData));
            insertTile(&sequenceBuffer[_nBrickVals*(brickIndex - firstBrickIndex)], mappedBuffer, atlasCoords, brickIndex);
        }
    }

    delete[] sequenceBuffer;

    
}

void ImageAtlasManager::removeFromAtlas(int brickIndex) {
    unsigned int atlasData = _brickMap[brickIndex];
    unsigned int atlasCoords = atlasData & 0x0FFFFFFF;
    _brickMap.erase(brickIndex);
    _freeAtlasCoords.push_back(atlasCoords);
    //_freeAtlasCoords.insert(_freeAtlasCoords.begin(), atlasCoords);
}
    
std::vector<unsigned int> ImageAtlasManager::atlasMap() {
    return _atlasMap;
}

unsigned int ImageAtlasManager::atlasMapBuffer() {
    return _atlasMapBuffer;
}

void ImageAtlasManager::pboToAtlas() {

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pboHandle);
    glm::size3_t dim = _textureAtlas->dimensions();
    glBindTexture(GL_TEXTURE_2D, *_textureAtlas);

    glTexSubImage2D(GL_TEXTURE_2D,
		    0,
		    0,
		    0,
		    dim[0],
		    dim[1],
		    GL_RED,
		    GL_FLOAT,
		    NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

ghoul::opengl::Texture* ImageAtlasManager::textureAtlas() {
    return _textureAtlas;
}

    /*glm::size3_t ImageAtlasManager::textureSize() {
    return _textureAtlas->dimensions();
    }*/

void ImageAtlasManager::insertTile(GLshort* in, GLfloat* out, unsigned int linearAtlasCoords, unsigned int brickIndex) {
    int x = linearAtlasCoords % _atlasBricksPerDim;
    int y = linearAtlasCoords / _atlasBricksPerDim;

    unsigned int xMin = x*_paddedBrickDims.x;
    unsigned int yMin = y*_paddedBrickDims.x;
    unsigned int xMax = xMin + _paddedBrickDims.y;
    unsigned int yMax = yMin + _paddedBrickDims.y;

    unsigned int from = 0;

    double expTime = _quadtreeList->exposureTime(brickIndex);
    double maxFlux = _quadtreeList->maxFlux();
    double lgMaxFlux = std::log10(_quadtreeList->maxFlux());

    for (unsigned int yValCoord = yMin; yValCoord<yMax; ++yValCoord) {
        for (unsigned int xValCoord = xMin; xValCoord<xMax; ++xValCoord) {
            unsigned int idx = xValCoord + yValCoord*_atlasWidth;
            double energy = in[from];
            double flux = energy /= expTime;
            out[idx] = std::log10(glm::clamp(flux, 1.0, maxFlux))/lgMaxFlux;
            from++;
        }
    }
}
    
} // namespace openspace
