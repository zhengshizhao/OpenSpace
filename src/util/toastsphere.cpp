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

#include <openspace/util/toastsphere.h>
#include <ghoul/logging/logmanager.h>

namespace {
	const std::string _loggerCat = "ToastSphere";
}

namespace openspace {

ToastSphere::ToastSphere(PowerScaledScalar radius, int levels) 
	: _radius(radius)
	, _levels(levels){}

bool ToastSphere::initialize() {
	createOctaHedron();
	return true;
}

void ToastSphere::render() {
	glBindVertexArray(_VAO);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)_quadrants.size() * 6);
	glBindVertexArray(0);
}

void ToastSphere::createOctaHedron() {
	float r = _radius[0];
	float w = _radius[1];

	// Octahedron vertices
	glm::vec4 v0 = glm::vec4(0, r, 0, w);
	glm::vec4 v1 = glm::vec4(0, 0, -r, w);
	glm::vec4 v2 = glm::vec4(r, 0, 0, w);
	glm::vec4 v3 = glm::vec4(0, 0, r, w);
	glm::vec4 v4 = glm::vec4(-r, 0, 0, w);
	glm::vec4 v5 = glm::vec4(0, -r, 0, w);

	// Toast texture coordinates. South pole (v5) gets split into four
	glm::vec2 tc0 =  glm::vec2(0.5,	0.5);
	glm::vec2 tc1 =  glm::vec2(0.5,	0);
	glm::vec2 tc2 =  glm::vec2(1,	0.5);
	glm::vec2 tc3 =  glm::vec2(0.5,	1);
	glm::vec2 tc4 =  glm::vec2(0,	0.5);
	glm::vec2 tc51 = glm::vec2(1,	0);
	glm::vec2 tc52 = glm::vec2(1,	1);
	glm::vec2 tc53 = glm::vec2(0,	1);
	glm::vec2 tc54 = glm::vec2(0,	0);

	// Octahedron quadrants
	_quadrants.push_back(Quadrant(v1, v2, v0, v5, tc1, tc2, tc0, tc51));
	_quadrants.push_back(Quadrant(v2, v3, v0, v5, tc2, tc3, tc0, tc52));
	_quadrants.push_back(Quadrant(v3, v4, v0, v5, tc3, tc4, tc0, tc53));
	_quadrants.push_back(Quadrant(v4, v1, v0, v5, tc4, tc1, tc0, tc54));

	// Subdivide
	if (_levels > 0) {
		std::vector<Quadrant> sQuadrants;
		for (int i = 0; i < _quadrants.size(); ++i) {
			std::vector<Quadrant> sQuadrant = subdivide(_quadrants[i], _levels);
			sQuadrants.insert(sQuadrants.end(), sQuadrant.begin(), sQuadrant.end());
		}
		_quadrants = sQuadrants;
	}

	// Prepare data for OpenGL
	std::vector<glm::vec4> vertexPosData;
	std::vector<glm::vec2> vertexToastData;
	for (Quadrant q : _quadrants) {
		vertexPosData.insert(vertexPosData.end(), q.vertices.begin(), q.vertices.end());
		vertexToastData.insert(vertexToastData.end(), q.toastCoords.begin(), q.toastCoords.end());
	}

	GLuint vertexPositionBuffer, vertexColorBuffer;
	glGenVertexArrays(1, &_VAO); // generate array
	glBindVertexArray(_VAO); // bind array

	// Vertex positions
	GLuint positionLocation = 0;
	glGenBuffers(1, &vertexPositionBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, vertexPosData.size()*sizeof(glm::vec4), &vertexPosData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(positionLocation);
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Vertex toastcoords
	GLuint tcLocation = 1;
	glGenBuffers(1, &vertexColorBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexColorBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, vertexToastData.size()*sizeof(glm::vec2), &vertexToastData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(tcLocation);
	glVertexAttribPointer(tcLocation, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

std::vector<Quadrant> ToastSphere::subdivide(Quadrant q, int levels) {
	// Positions
	glm::vec4 p0 = q.vertices[0];
	glm::vec4 p1 = q.vertices[1];
	glm::vec4 p2 = q.vertices[2];
	glm::vec4 p3 = q.vertices[4];

	float radius = glm::length((glm::vec3)p0.xyz);
	float pss = p0.w;

	glm::vec4 p01 = glm::vec4(radius * glm::normalize(p0.xyz + p1.xyz), pss);
	glm::vec4 p20 = glm::vec4(radius * glm::normalize(p2.xyz + p0.xyz), pss);
	glm::vec4 p30 = glm::vec4(radius * glm::normalize(p3.xyz + p0.xyz), pss);
	glm::vec4 p12 = glm::vec4(radius * glm::normalize(p1.xyz + p2.xyz), pss);
	glm::vec4 p31 = glm::vec4(radius * glm::normalize(p3.xyz + p1.xyz), pss);

	// Toast coordinates
	glm::vec2 tc0 = q.toastCoords[0];
	glm::vec2 tc1 = q.toastCoords[1];
	glm::vec2 tc2 = q.toastCoords[2];
	glm::vec2 tc3 = q.toastCoords[4];
	glm::vec2 tc01 = 0.5f*(tc0 + tc1);
	glm::vec2 tc20 = 0.5f*(tc2 + tc0);
	glm::vec2 tc30 = 0.5f*(tc3 + tc0);
	glm::vec2 tc12 = 0.5f*(tc1 + tc2);
	glm::vec2 tc31 = 0.5f*(tc3 + tc1);

	// New quadrants
	std::vector<Quadrant> quadrants;
	Quadrant q1 = Quadrant(p20, p12, p2, p01, tc20, tc12, tc2, tc01);
	Quadrant q2 = Quadrant(p01, p1, p12, p31, tc01, tc1, tc12, tc31);
	Quadrant q3 = Quadrant(p0, p01, p20, p30, tc0, tc01, tc20, tc30);
	Quadrant q4 = Quadrant(p30, p31, p01, p3, tc30, tc31, tc01, tc3);

	levels--;
	if (levels > 0) { // We need to go deeper
		std::vector<Quadrant> qs1, qs2, qs3, qs4;
		qs1 = subdivide(q1, levels);
		qs2 = subdivide(q2, levels);
		qs3 = subdivide(q3, levels);
		qs4 = subdivide(q4, levels);
		quadrants.insert(quadrants.end(), qs1.begin(), qs1.end());
		quadrants.insert(quadrants.end(), qs2.begin(), qs2.end());
		quadrants.insert(quadrants.end(), qs3.begin(), qs3.end());
		quadrants.insert(quadrants.end(), qs4.begin(), qs4.end());
	} else {
		quadrants.push_back(q1);
		quadrants.push_back(q2);
		quadrants.push_back(q3);
		quadrants.push_back(q4);
	}

	return quadrants;
}

} //namespace openspace