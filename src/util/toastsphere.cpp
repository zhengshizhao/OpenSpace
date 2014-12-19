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

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/io/texture/texturereader.h>

namespace {
	const std::string _loggerCat = "ToastSphere";
}

namespace openspace {

namespace constants {
namespace toastsphere {
const std::string keyRadius = "Radius";
const std::string keyLevels = "Levels";
} // namespace simplespheregeometry
}

namespace planetgeometry {

ToastSphere::ToastSphere(const ghoul::Dictionary& dictionary)
	: PlanetGeometry()
	, _radius("radius", "Radius", glm::vec2(1.f, 0.f), glm::vec2(-10.f, -20.f),
		glm::vec2(10.f, 20.f))
	, _maxLevel("maxLevels", "MaxLevels", 2, 0, 5)
	, _currentLevel("currentLevel", "CurrentLevel", 2, 0, 5)
	, _numVertices(0)
	, _texture(nullptr) {

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
	} else
		_radius = radius;

	double levels;
	success = dictionary.getValue(keyLevels, levels);
	if (!success) {
		LERROR("ToastSphere of '" << name << "' did not provide a key '"
			<< keyLevels << "'");
	} else
		_maxLevel = static_cast<int>(levels);

	addProperty(_radius);
	_radius.onChange(std::bind(&ToastSphere::createOctaHedron, this));
	addProperty(_maxLevel);
	_maxLevel.onChange(std::bind(&ToastSphere::createOctaHedron, this));
	addProperty(_currentLevel);
	_currentLevel.onChange(std::bind(&ToastSphere::updateDetailLevel, this));
}

ToastSphere::~ToastSphere(){}

bool ToastSphere::initialize(RenderablePlanet* parent) {
	bool success = PlanetGeometry::initialize(parent);	
	createOctaHedron();
	return success;
}

void ToastSphere::deinitialize() {
	for (ToastQuadrant* q : _quadrants)
		delete q;

	_quadrants.clear();
}

void ToastSphere::bindTexture(ghoul::opengl::ProgramObject* programObject) {
	// Set the Program object pointer needed for texture binding
	// The actual texture binding is done by each quadrant before drawing
	for (ToastQuadrant* q : _quadrants)
		q->setProgramObject(programObject);
}

void ToastSphere::loadTexture() {
	// TODO Gets called when texture path in renderableplanet is changed
	// Send the path to the quadrants and re-load textures

	//for (ToastQuadrant* q : _quadrants)
	//	q->loadTexture(_currentLevel);
}

void ToastSphere::render() {
	for (ToastQuadrant* q : _quadrants)
		q->draw(_currentLevel);
}

void ToastSphere::setDetailLevel(int level) {		
	_currentLevel = level;
	updateDetailLevel();
}

void ToastSphere::updateDetailLevel() {
	for (ToastQuadrant* q : _quadrants)
		q->updateDetailLevel(_currentLevel);
}

void ToastSphere::createOctaHedron() {
	for (ToastQuadrant* q : _quadrants)
		delete q;

	_quadrants.clear();

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

	// Tile texture coordinates
	glm::vec2 s0t0 = glm::vec2(0, 0);
	glm::vec2 s1t0 = glm::vec2(1, 0);
	glm::vec2 s0t1 = glm::vec2(0, 1);
	glm::vec2 s1t1 = glm::vec2(1, 1);

	//// The 4 level-0 Octahedron quadrants
	//_quadrants.push_back(new ToastQuadrant(v1, v2, v0, v5, tc1, tc2, tc0, tc51, 0));
	//_quadrants.push_back(new ToastQuadrant(v2, v3, v0, v5, tc2, tc3, tc0, tc52, 0));
	//_quadrants.push_back(new ToastQuadrant(v3, v4, v0, v5, tc3, tc4, tc0, tc53, 0));
	//_quadrants.push_back(new ToastQuadrant(v4, v1, v0, v5, tc4, tc1, tc0, tc54, 0));
	
	_quadrants.push_back(new ToastQuadrant(v1, v2, v0, v5, 
		s0t0, s1t1, s0t1, s1t0, glm::ivec2(0, 1), 0, 0));	
	
	_quadrants.push_back(new ToastQuadrant(v2, v3, v0, v5, 
		s1t0, s0t1, s0t0, s1t1, glm::ivec2(1, 1), 1, 0));
	
	_quadrants.push_back(new ToastQuadrant(v3, v4, v0, v5, 
		s1t1, s0t0, s1t0, s0t1, glm::ivec2(1, 0), 2, 0));
	
	_quadrants.push_back(new ToastQuadrant(v4, v1, v0, v5, 
		s0t1, s1t0, s1t1, s0t0, glm::ivec2(0, 0), 3, 0));

	// Subdivide
	for (ToastQuadrant* q : _quadrants) {
		q->subdivide(_maxLevel);
		q->updateDetailLevel(_currentLevel);
	}
}

} // namespace planetgeometry
} // namespace openspace