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

#include <openspace/util/constants.h>
#include <openspace/util/toastsphere.h>
#include <openspace/util/toastquadrant.h>
#include <ghoul/logging/logmanager.h>

namespace {
	const std::string _loggerCat = "ToastSphere";
}

namespace openspace {

namespace constants {
namespace toastsphere {
const std::string keyRadius = "Radius";
const std::string keyLevels = "Levels";
}  // namespace simplespheregeometry
}

namespace planetgeometry {

ToastSphere::ToastSphere(const ghoul::Dictionary& dictionary)
	: PlanetGeometry()
	, _radius("radius", "Radius", glm::vec2(1.f, 0.f), glm::vec2(-10.f, -20.f),
		glm::vec2(10.f, 20.f))
	, _levels("levels", "Levels", 3, 1, 10)
	, _numVertices(0) {

	using constants::scenegraphnode::keyName;
	using constants::toastsphere::keyRadius;
	using constants::toastsphere::keyLevels;

	// The name is passed down from the SceneGraphNode
	std::string name;
	bool success = dictionary.getValue(keyName, name);
	assert(success);

	glm::vec2 radius;
	success = dictionary.getValue(keyRadius, radius);
	if (!success) {
		LERROR("ToastSphere of '" << name << "' did not provide a key '"
			<< keyRadius << "'");
	}
	else
		_radius = radius;

	double levels;
	success = dictionary.getValue(keyLevels, levels);
	if (!success) {
		LERROR("ToastSphere of '" << name << "' did not provide a key '"
			<< keyLevels << "'");
	}
	else
		_levels = static_cast<int>(levels);

	addProperty(_radius);
	_radius.onChange(std::bind(&ToastSphere::createOctaHedron, this));
	addProperty(_levels);
	_levels.onChange(std::bind(&ToastSphere::createOctaHedron, this));
}

ToastSphere::~ToastSphere(){}

bool ToastSphere::initialize(RenderablePlanet* parent) {
	bool success = PlanetGeometry::initialize(parent);	
	createOctaHedron();
	return success;
}

void ToastSphere::deinitialize() {
	for (ToastQuadrant* q : _quadrants) {
		delete q;
	}
}

void ToastSphere::render() {
	glBindVertexArray(_VAO);
	glDrawArrays(GL_TRIANGLES, 0, _numVertices);
	glBindVertexArray(0);
}

void ToastSphere::createOctaHedron() {
	PowerScaledScalar planetSize(_radius);
	_parent->setBoundingSphere(planetSize);

	float r = planetSize[0];
	float w = planetSize[1];

	// Octahedron vertices
	glm::vec4 v0 = glm::vec4(0, r, 0, w);
	glm::vec4 v1 = glm::vec4(0, 0, -r, w);
	glm::vec4 v2 = glm::vec4(r, 0, 0, w);
	glm::vec4 v3 = glm::vec4(0, 0, r, w);
	glm::vec4 v4 = glm::vec4(-r, 0, 0, w);
	glm::vec4 v5 = glm::vec4(0, -r, 0, w);

	// Toast texture coordinates. South pole (v5) gets split into four
	glm::vec2 tc0 = glm::vec2(0.5, 0.5);
	glm::vec2 tc1 = glm::vec2(0.5, 0);
	glm::vec2 tc2 = glm::vec2(1, 0.5);
	glm::vec2 tc3 = glm::vec2(0.5, 1);
	glm::vec2 tc4 = glm::vec2(0, 0.5);
	glm::vec2 tc51 = glm::vec2(1, 0);
	glm::vec2 tc52 = glm::vec2(1, 1);
	glm::vec2 tc53 = glm::vec2(0, 1);
	glm::vec2 tc54 = glm::vec2(0, 0);

	// Octahedron quadrants
	_quadrants.push_back(new ToastQuadrant(v1, v2, v0, v5, tc1, tc2, tc0, tc51));
	_quadrants.push_back(new ToastQuadrant(v2, v3, v0, v5, tc2, tc3, tc0, tc52));
	_quadrants.push_back(new ToastQuadrant(v3, v4, v0, v5, tc3, tc4, tc0, tc53));
	_quadrants.push_back(new ToastQuadrant(v4, v1, v0, v5, tc4, tc1, tc0, tc54));

	// Subdivide
	if (_levels > 0) {
		_quadrants[0]->subdivide(_levels);
		_quadrants[1]->subdivide(_levels);
		_quadrants[2]->subdivide(_levels);
		_quadrants[3]->subdivide(_levels);
	}

	// Prepare data for OpenGL
	std::vector<glm::vec4> vertexPosData, qVerts;
	std::vector<glm::vec2> vertexToastData, qToastcoords;
	for (ToastQuadrant* q : _quadrants) {
		qVerts = q->getVertices();
		qToastcoords = q->getToastCoords();
		vertexPosData.insert(vertexPosData.end(), qVerts.begin(), qVerts.end());
		vertexToastData.insert(vertexToastData.end(), qToastcoords.begin(), qToastcoords.end());
	}

	_numVertices = (GLsizei)vertexPosData.size();

	GLuint vertexPositionBuffer, vertexColorBuffer;
	glGenVertexArrays(1, &_VAO); // generate array
	glBindVertexArray(_VAO); // bind array

	// Vertex positions
	GLuint positionLocation = 0;
	glGenBuffers(1, &vertexPositionBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, _numVertices*sizeof(glm::vec4), &vertexPosData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(positionLocation);
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Vertex toastcoords
	GLuint tcLocation = 1;
	glGenBuffers(1, &vertexColorBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexColorBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, _numVertices*sizeof(glm::vec2), &vertexToastData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(tcLocation);
	glVertexAttribPointer(tcLocation, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

} // namespace planetgeometry
} //namespace openspace