/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014                                                                    *
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

#ifndef __TOASTQUADRANT_H__
#define __TOASTQUADRANT_H__

#include <ghoul/opengl/ghoul_gl.h>
#include <glm/glm.hpp>
#include <vector>

namespace ghoul {
	namespace opengl {
		class ProgramObject;
		class Texture;
	}
}

namespace openspace {

class ToastQuadrant{
public:
	ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
		glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3, 
		glm::ivec2 tilePos, int rootQuadrant, int level);

	ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
		glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3,
		glm::ivec2 tilePos, int rootQuadrant, int level,
		ToastQuadrant* parent);
	~ToastQuadrant();

	void subdivide(int levels = 1);	
	void draw(int detailLevel);
	void updateDetailLevel(int newLevel);

	bool isLeaf() { return _isLeaf; };
	void setProgramObject(ghoul::opengl::ProgramObject* programObject);
	int getRootQuadrant() { return _rootQuadrant; };

private:
	void bindTileTexture();
	void loadTileTexture();

	void generateOpenGLData();
	void cleanupOpenGLData();

	GLuint _VAO, _vertexPositionBuffer, _vertexToastcoordBuffer;
	std::vector<glm::vec4> _vertices;
	std::vector<glm::vec2> _toastCoords;
	std::vector<ToastQuadrant*> _children;
	ToastQuadrant* _parent;
	ghoul::opengl::Texture* _texture;
	int _level, _rootQuadrant;
	bool _isLeaf, _beingDrawn, _tileLoaded;
	ghoul::opengl::ProgramObject* _programObject;
	std::string _texturePath;
	glm::ivec2 _tilePos;
};

} // namespace openspace

#endif //__TOASTQUADRANT_H__