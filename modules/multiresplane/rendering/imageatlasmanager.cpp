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

namespace {
    const std::string _loggerCat = "ImageAtlasManager";
}

namespace openspace {

ImageAtlasManager::ImageAtlasManager(QuadtreeList* qtl) {
    _quadtreeList = qtl;
}

ImageAtlasManager::~ImageAtlasManager() {}

void ImageAtlasManager::updateAtlas(std::vector<int>& brickIndices) {}

void ImageAtlasManager::addToAtlas(int firstBrickIndex, int lastBrickIndex, float* mappedBuffer) {}

void ImageAtlasManager::removeFromAtlas(int brickIndex) {}

bool ImageAtlasManager::initialize() {}

std::vector<unsigned int> ImageAtlasManager::atlasMap() {}

unsigned int ImageAtlasManager::atlasMapBuffer() {}

void ImageAtlasManager::pboToAtlas() {}

ghoul::opengl::Texture* ImageAtlasManager::textureAtlas() {}

glm::size2_t ImageAtlasManager::textureSize() {}

void ImageAtlasManager::fillImage(float* in, float* out, unsigned int linearAtlasCoords) {}

} // namespace openspace
