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
#include <openspace/util/toastquadrant.h>

#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/io/texture/texturereader.h>

#include <sstream>

namespace {
	const std::string _loggerCat = "ToastQuadrant";
}

namespace openspace {

ToastQuadrant::ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3, 
	glm::ivec2 tilePos, int rootQuadrant, int level)
	: _isLeaf(true), _level(level), _parent(nullptr), _texture(nullptr), 
	_tilePos(tilePos), _beingDrawn(false), _tileLoaded(false),
	_rootQuadrant(rootQuadrant) {

	_vertices.push_back(p0);
	_vertices.push_back(p1);
	_vertices.push_back(p2);
	_vertices.push_back(p0);
	_vertices.push_back(p3);
	_vertices.push_back(p1);

	_toastCoords.push_back(tc0);
	_toastCoords.push_back(tc1);
	_toastCoords.push_back(tc2);
	_toastCoords.push_back(tc0);
	_toastCoords.push_back(tc3);
	_toastCoords.push_back(tc1);

	generateOpenGLData();

	// I know, I know
	_texturePath = "C:/tiles/earth_bluemarble_toast/";
}

ToastQuadrant::ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3, 
	glm::ivec2 tilePos, int rootQuadrant, int level,
	ToastQuadrant* parent) : _isLeaf(true), _level(level), _parent(parent), 
	_texture(nullptr), _tilePos(tilePos), _beingDrawn(false), _tileLoaded(false),
	_rootQuadrant(rootQuadrant) {

	_vertices.push_back(p0);
	_vertices.push_back(p1);
	_vertices.push_back(p2);
	_vertices.push_back(p0);
	_vertices.push_back(p3);
	_vertices.push_back(p1);

	_toastCoords.push_back(tc0);
	_toastCoords.push_back(tc1);
	_toastCoords.push_back(tc2);
	_toastCoords.push_back(tc0);
	_toastCoords.push_back(tc3);
	_toastCoords.push_back(tc1);

	generateOpenGLData();

	// I know, I know
	_texturePath = "C:/tiles/earth_bluemarble_toast/";
}

ToastQuadrant::~ToastQuadrant() {
	for (ToastQuadrant* q : _children)
		delete q;

	_children.clear();
	_vertices.clear();
	_toastCoords.clear();
	cleanupOpenGLData();
}

void ToastQuadrant::subdivide(int levels) {
	if (levels < 1)
		return;

	// Positions
	glm::vec4 p0 = _vertices[0];
	glm::vec4 p1 = _vertices[1];
	glm::vec4 p2 = _vertices[2];
	glm::vec4 p3 = _vertices[4];

	float radius = glm::length((glm::vec3)p0.xyz);
	float pss = p0.w;

	glm::vec4 p01 = glm::vec4(radius * glm::normalize(p0.xyz + p1.xyz), pss);
	glm::vec4 p20 = glm::vec4(radius * glm::normalize(p2.xyz + p0.xyz), pss);
	glm::vec4 p30 = glm::vec4(radius * glm::normalize(p3.xyz + p0.xyz), pss);
	glm::vec4 p12 = glm::vec4(radius * glm::normalize(p1.xyz + p2.xyz), pss);
	glm::vec4 p31 = glm::vec4(radius * glm::normalize(p3.xyz + p1.xyz), pss);

	// Toast coordinates
	glm::vec2 tc0 = _toastCoords[0];
	glm::vec2 tc1 = _toastCoords[1];
	glm::vec2 tc2 = _toastCoords[2];
	glm::vec2 tc3 = _toastCoords[4];
	glm::vec2 tc01 = 0.5f*(tc0 + tc1);
	glm::vec2 tc20 = 0.5f*(tc2 + tc0);
	glm::vec2 tc30 = 0.5f*(tc3 + tc0);
	glm::vec2 tc12 = 0.5f*(tc1 + tc2);
	glm::vec2 tc31 = 0.5f*(tc3 + tc1);
	
	// Tile texture coordinates
	glm::vec2 s0t1 = _toastCoords[0];
	glm::vec2 s1t0 = _toastCoords[1];
	glm::vec2 s0t0 = _toastCoords[2];
	glm::vec2 s1t1 = _toastCoords[4];

	glm::ivec2 tilePos0, tilePos1, tilePos2, tilePos3;
	if (_rootQuadrant == 1 || _rootQuadrant == 3){
		tilePos0 = _tilePos * 2 + (glm::ivec2)s0t0;
		tilePos1 = _tilePos * 2 + (glm::ivec2)s0t1;
		tilePos2 = _tilePos * 2 + (glm::ivec2)s1t0;
		tilePos3 = _tilePos * 2 + (glm::ivec2)s1t1;
	} else if (_rootQuadrant == 0 || _rootQuadrant == 2) {
		tilePos0 = _tilePos * 2 + (glm::ivec2)s1t1;
		tilePos1 = _tilePos * 2 + (glm::ivec2)s1t0;
		tilePos2 = _tilePos * 2 + (glm::ivec2)s0t1;
		tilePos3 = _tilePos * 2 + (glm::ivec2)s0t0;
	}
	
	_isLeaf = false;

	// Child quadrants
	//_children.push_back(new ToastQuadrant(p20, p12, p2, p01, tc20, tc12, tc2, tc01, _level+1, this));
	//_children.push_back(new ToastQuadrant(p01, p1, p12, p31, tc01, tc1, tc12, tc31, _level+1, this));
	//_children.push_back(new ToastQuadrant(p0, p01, p20, p30, tc0, tc01, tc20, tc30, _level+1, this));
	//_children.push_back(new ToastQuadrant(p30, p31, p01, p3, tc30, tc31, tc01, tc3, _level+1, this));
	_children.push_back(new ToastQuadrant(p20, p12, p2, p01, 
		s0t1, s1t0, s0t0, s1t1, tilePos0, _rootQuadrant, _level + 1, this));
	
	_children.push_back(new ToastQuadrant(p01, p1, p12, p31, 
		s0t1, s1t0, s0t0, s1t1, tilePos1, _rootQuadrant, _level + 1, this));
	
	_children.push_back(new ToastQuadrant(p0, p01, p20, p30, 
		s0t1, s1t0, s0t0, s1t1, tilePos2, _rootQuadrant, _level + 1, this));
	
	_children.push_back(new ToastQuadrant(p30, p31, p01, p3, 
		s0t1, s1t0, s0t0, s1t1, tilePos3, _rootQuadrant, _level + 1, this));

	levels--;
	if (levels > 0) { // We need to go deeper
		_children[0]->subdivide(levels);
		_children[1]->subdivide(levels);
		_children[2]->subdivide(levels);
		_children[3]->subdivide(levels);
	}
}

void ToastQuadrant::setProgramObject(ghoul::opengl::ProgramObject* programObject) {
	if (_isLeaf || _beingDrawn)
		_programObject = programObject;
	else {
		for (ToastQuadrant* q : _children)
			q->setProgramObject(programObject);
	}
}

void ToastQuadrant::bindTileTexture() {
	ghoul::opengl::TextureUnit unit;
	unit.activate();
	_texture->bind();
	_programObject->setUniform("texture1", unit);
}

void ToastQuadrant::loadTileTexture() {
	delete _texture;
	_texture = nullptr;
	std::stringstream ss;
	ss << _level + 1 << "/" << _tilePos.x << "/" << _tilePos.x << "_" << _tilePos.y;
	std::string tilePosition = ss.str();

	std::string tilePath = _texturePath + tilePosition + ".png";

	_texture = ghoul::io::TextureReader::ref().loadTexture(tilePath);
	if (_texture) {
		LDEBUG("Loaded texture from '" + tilePath + "'");
		_texture->uploadTexture();

		// Textures of planets looks much smoother with AnisotropicMipMap rather than linear
		_texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
	}
}

void ToastQuadrant::draw(int detailLevel) {
	if (_isLeaf || detailLevel == _level) {
		if (!_tileLoaded) {
			loadTileTexture();
			_tileLoaded = true;
		}
			
		bindTileTexture();

		glBindVertexArray(_VAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	} else {
		for (ToastQuadrant* q : _children)
			q->draw(detailLevel);
	}
}

void ToastQuadrant::updateDetailLevel(int newLevel) {
	// Cleanup OpenGL data for old level
	if (_isLeaf || _beingDrawn) {
		cleanupOpenGLData();
		_beingDrawn = false;
	} else {
		for (ToastQuadrant* q : _children)
			q->updateDetailLevel(newLevel);
	}

	// Generate OpenGL data for new level
	if (_isLeaf || newLevel == _level) {
		generateOpenGLData();
		_beingDrawn = true;
	}
	else {
		for (ToastQuadrant* q : _children)
			q->updateDetailLevel(newLevel);
	}
}

void ToastQuadrant::generateOpenGLData() {
	glGenVertexArrays(1, &_VAO); // generate array
	glBindVertexArray(_VAO); // bind array

	// Vertex positions
	GLuint positionLocation = 0;
	glGenBuffers(1, &_vertexPositionBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, 6*sizeof(glm::vec4), &_vertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(positionLocation); // Set location
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Vertex toastcoords
	GLuint tcLocation = 1;
	glGenBuffers(1, &_vertexToastcoordBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vertexToastcoordBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, 6*sizeof(glm::vec2), &_toastCoords[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(tcLocation); // Set location
	glVertexAttribPointer(tcLocation, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

void ToastQuadrant::cleanupOpenGLData() {
	if (_vertexPositionBuffer)
		glDeleteBuffers(1, &_vertexPositionBuffer);
	if (_vertexToastcoordBuffer)
		glDeleteBuffers(1, &_vertexToastcoordBuffer);
	if (_VAO)
		glDeleteVertexArrays(1, &_VAO);

	_vertexPositionBuffer = GL_FALSE;
	_vertexToastcoordBuffer = GL_FALSE;
	_VAO = GL_FALSE;
}

} //namespace openspace