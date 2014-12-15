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

ToastQuadrant::ToastQuadrant(psc p0, psc p1, psc p2, psc p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3) 
	: _isLeaf(true), _parent(nullptr) {
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

ToastQuadrant::ToastQuadrant(psc p0, psc p1, psc p2, psc p3,
	glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3,
	ToastQuadrant* parent) : _isLeaf(true), _parent(parent) {
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

} //namespace openspace