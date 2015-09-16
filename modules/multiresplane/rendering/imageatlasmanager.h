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

#ifndef __IMAGEATLASMANAGER_H__
#define __IMAGEATLASMANAGER_H__

#include <modules/multiresplane/rendering/quadtreelist.h>
#include <ghoul/glm.h>
#include <glm/gtx/std_based_type.hpp>

#include <string>
#include <vector>
#include <climits>
#include <map>
#include <set>

namespace ghoul {
    namespace opengl {
        class Texture;
    }
}

namespace openspace {

class ImageAtlasManager {
public:
    ImageAtlasManager(QuadtreeList* qtl, unsigned int atlasCapacity);
    ~ImageAtlasManager();

    void updateAtlas(std::vector<int>& brickIndices);
    void addToAtlas(int firstBrickIndex, int lastBrickIndex, GLfloat* mappedBuffer);
    void removeFromAtlas(int brickIndex);
    bool initialize();
    std::vector<unsigned int> atlasMap();
    unsigned int atlasMapBuffer();

    void pboToAtlas();
    ghoul::opengl::Texture* textureAtlas();
    //glm::size2_t textureSize();
    
    void setAtlasCapacity(unsigned int atlasCapacity);
private:
    const unsigned int NOT_USED = UINT_MAX;
    QuadtreeList* _quadtreeList;

    std::vector<unsigned int> _atlasMap;
    std::map<unsigned int, unsigned int> _brickMap;
    std::vector<unsigned int> _freeAtlasCoords;
    std::set<unsigned int> _requiredBricks;
    std::set<unsigned int> _prevRequiredBricks;

    ghoul::opengl::Texture* _textureAtlas;

    unsigned int _nBricksPerDim,
                 _atlasBricksPerDim,
                 _nQtLeaves,
                 _nQtNodes,
                 _nQtLevels,
                 _nBrickVals,
                 _brickSize,
                 _atlasCapacity,
                 _nBricksInMap,
                 _atlasDim,
                 _atlasWidth,
                 _atlasHeight,
                 _atlasSize;
        

    glm::ivec2 _paddedBrickDims;
    GLuint _pboHandle;
    GLuint _atlasMapBuffer;
    
    bool _needsReinitialization;

    void insertTile(GLshort* in, GLfloat* out, unsigned int linearAtlasCoords, unsigned int brickIndex);
};

} // namespace openspace

#endif // __IMAGEATLASMANAGER_H__
