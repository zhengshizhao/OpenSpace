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

namespace openspace {

ToastQuadrant::ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3, int level) 
	: _isLeaf(true), _level(level), _parent(nullptr) {
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
}

ToastQuadrant::ToastQuadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3, int level,
	ToastQuadrant* parent) : _isLeaf(true), _level(level), _parent(parent) {
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
}

ToastQuadrant::~ToastQuadrant() {
	for (ToastQuadrant* q : _children) {
		delete q;
	}
}

void ToastQuadrant::subdivide(int levels) {	
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

	// Child quadrants
	_children.push_back(new ToastQuadrant(p20, p12, p2, p01, tc20, tc12, tc2, tc01, _level+1, this));
	_children.push_back(new ToastQuadrant(p01, p1, p12, p31, tc01, tc1, tc12, tc31, _level+1, this));
	_children.push_back(new ToastQuadrant(p0, p01, p20, p30, tc0, tc01, tc20, tc30, _level+1, this));
	_children.push_back(new ToastQuadrant(p30, p31, p01, p3, tc30, tc31, tc01, tc3, _level+1, this));
	_isLeaf = false;

	levels--;
	if (levels > 0) { // We need to go deeper
		_children[0]->subdivide(levels);
		_children[1]->subdivide(levels);
		_children[2]->subdivide(levels);
		_children[3]->subdivide(levels);
	}
}

std::vector<glm::vec4> ToastQuadrant::getVertices(int detailLevel) {
	if (_isLeaf || detailLevel == _level) {
		return _vertices;
	} else {
		std::vector<glm::vec4> verticeData, qVerts;
		for (ToastQuadrant* q : _children) {
			qVerts = q->getVertices(detailLevel);
			verticeData.insert(verticeData.end(), qVerts.begin(), qVerts.end());
		}
		return verticeData;
	}
}

std::vector<glm::vec2> ToastQuadrant::getToastCoords(int detailLevel) {
	if (_isLeaf || detailLevel == _level) {
		return _toastCoords;
	} else {
		std::vector<glm::vec2> toastcoordData, qToastcoords;
		for (ToastQuadrant* q : _children) {
			qToastcoords = q->getToastCoords(detailLevel);
			toastcoordData.insert(toastcoordData.end(), qToastcoords.begin(), qToastcoords.end());
		}
		return toastcoordData;
	}
}

} //namespace openspace