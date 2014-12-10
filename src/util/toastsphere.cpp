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

ToastSphere::ToastSphere(PowerScaledScalar radius) {
	_radius = radius;
}

bool ToastSphere::initialize() {
	createOctaHedron();
	return true;
}

void ToastSphere::render() {
	glBindVertexArray(_VAO);
	glDrawArrays(GL_TRIANGLES, 0, _quadrants.size() * 6);
	glBindVertexArray(0);
}

void ToastSphere::createOctaHedron() {
	float r = _radius[0];
	int w = _radius[1];
	int levels = 3;

	glm::vec4 v0 = glm::vec4(0, r, 0, w);
	glm::vec4 v1 = glm::vec4(0, 0, r, w);
	glm::vec4 v2 = glm::vec4(r, 0, 0, w);
	glm::vec4 v3 = glm::vec4(0, 0, -r, w);
	glm::vec4 v4 = glm::vec4(-r, 0, 0, w);
	glm::vec4 v5 = glm::vec4(0, -r, 0, w);

	_quadrants.push_back(Quadrant(v1, v2, v0, v5));
	_quadrants.push_back(Quadrant(v2, v3, v0, v5));
	_quadrants.push_back(Quadrant(v3, v4, v0, v5));
	_quadrants.push_back(Quadrant(v4, v1, v0, v5));

	if (levels > 0) {
		std::vector<Quadrant> sQuadrants;
		for (int i = 0; i < _quadrants.size(); ++i) {
			std::vector<Quadrant> sQuadrant = subdivide(_quadrants[i], levels);
			sQuadrants.insert(sQuadrants.end(), sQuadrant.begin(), sQuadrant.end());
		}
		_quadrants = sQuadrants;
	}

	GLuint vertexPositionBuffer;
	glGenVertexArrays(1, &_VAO); // generate array
	glBindVertexArray(_VAO); // bind array
	glGenBuffers(1, &vertexPositionBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, _quadrants.size()*sizeof(Quadrant), &_quadrants[0], GL_STATIC_DRAW);

	// Vertex positions
	GLuint vertexLocation = 0;
	glEnableVertexAttribArray(vertexLocation);
	glVertexAttribPointer(vertexLocation, 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

std::vector<Quadrant> ToastSphere::subdivide(Quadrant q, int levels) {
	glm::vec4 p0 = q.v0;
	glm::vec4 p1 = q.v1;
	glm::vec4 p2 = q.v2;
	glm::vec4 p3 = q.v4;

	float radius = glm::length((glm::vec3)p0.xyz);
	float pss = p0.w;

	glm::vec4 p01 = glm::vec4(radius * glm::normalize(p0.xyz + p1.xyz), pss);
	glm::vec4 p20 = glm::vec4(radius * glm::normalize(p2.xyz + p0.xyz), pss);
	glm::vec4 p30 = glm::vec4(radius * glm::normalize(p3.xyz + p0.xyz), pss);
	glm::vec4 p12 = glm::vec4(radius * glm::normalize(p1.xyz + p2.xyz), pss);
	glm::vec4 p31 = glm::vec4(radius * glm::normalize(p3.xyz + p1.xyz), pss);

	std::vector<Quadrant> quadrants;
	Quadrant q1 = Quadrant(p20, p12, p2, p01);
	Quadrant q2 = Quadrant(p01, p1, p12, p31);
	Quadrant q3 = Quadrant(p0, p01, p20, p30);
	Quadrant q4 = Quadrant(p30, p31, p01, p3);

	levels--;
	if (levels > 0) {
		std::vector<Quadrant> qs1, qs2, qs3, qs4;
		qs1 = subdivide(q1, levels);
		qs2 = subdivide(q2, levels);
		qs3 = subdivide(q3, levels);
		qs4 = subdivide(q4, levels);
		quadrants.insert(quadrants.end(), qs1.begin(), qs1.end());
		quadrants.insert(quadrants.end(), qs2.begin(), qs2.end());
		quadrants.insert(quadrants.end(), qs3.begin(), qs3.end());
		quadrants.insert(quadrants.end(), qs4.begin(), qs4.end());
	}
	else {
		quadrants.push_back(Quadrant(p20, p12, p2, p01));
		quadrants.push_back(Quadrant(p01, p1, p12, p31));
		quadrants.push_back(Quadrant(p0, p01, p20, p30));
		quadrants.push_back(Quadrant(p30, p31, p01, p3));
	}

	return quadrants;
}

} //namespace openspace