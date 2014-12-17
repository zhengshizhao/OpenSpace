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
	, _maxLevel("maxLevels", "MaxLevels", 3, 0, 10)
	, _currentLevel("currentLevel", "CurrentLevel", 3, 0, 10)
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
	loadTexture();
	return success;
}

void ToastSphere::deinitialize() {
	for (ToastQuadrant* q : _quadrants) {
		delete q;
	}
	_quadrants.clear();
}

void ToastSphere::bindTexture(ghoul::opengl::ProgramObject* programObject) {
	ghoul::opengl::TextureUnit unit;
	unit.activate();
	_texture->bind();
	programObject->setUniform("texture1", unit);
}

void ToastSphere::loadTexture() {
	delete _texture;
	_texture = nullptr;
	properties::StringProperty texturePath = _parent->getTexturePath();
	if (texturePath.value() != "") {
		_texture = ghoul::io::TextureReader::ref().loadTexture(absPath(texturePath));
		if (_texture) {
			LDEBUG("Loaded texture from '" << absPath(texturePath) << "'");
			_texture->uploadTexture();

			// Textures of planets looks much smoother with AnisotropicMipMap rather than linear
			_texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
		}
	}	
}

void ToastSphere::render() {
	glBindVertexArray(_VAO);
	glDrawArrays(GL_TRIANGLES, 0, _numVertices);
	glBindVertexArray(0);
}

void ToastSphere::setDetailLevel(int level) {	
	_currentLevel = level;
	updateDetailLevel();
}

void ToastSphere::updateDetailLevel() {
	cleanupOpenGLData();
	generateOpenGLData(_currentLevel);
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

	// The 4 level-0 Octahedron quadrants
	_quadrants.push_back(new ToastQuadrant(v1, v2, v0, v5, tc1, tc2, tc0, tc51, 0));
	_quadrants.push_back(new ToastQuadrant(v2, v3, v0, v5, tc2, tc3, tc0, tc52, 0));
	_quadrants.push_back(new ToastQuadrant(v3, v4, v0, v5, tc3, tc4, tc0, tc53, 0));
	_quadrants.push_back(new ToastQuadrant(v4, v1, v0, v5, tc4, tc1, tc0, tc54, 0));

	// Subdivide
	if (_maxLevel > 0) {
		_quadrants[0]->subdivide(_maxLevel);
		_quadrants[1]->subdivide(_maxLevel);
		_quadrants[2]->subdivide(_maxLevel);
		_quadrants[3]->subdivide(_maxLevel);
	}

	// Prepare data for OpenGL
	cleanupOpenGLData();
	generateOpenGLData(_currentLevel);
}

void ToastSphere::generateOpenGLData(int detailLevel) {
	LDEBUG("generateOpenGLData for level: " << detailLevel);
	std::vector<glm::vec4> vertexPosData, qVerts;
	std::vector<glm::vec2> vertexToastData, qToastcoords;
	for (ToastQuadrant* q : _quadrants) {
		qVerts = q->getVertices(detailLevel);
		qToastcoords = q->getToastCoords(detailLevel);
		vertexPosData.insert(vertexPosData.end(), qVerts.begin(), qVerts.end());
		vertexToastData.insert(vertexToastData.end(), qToastcoords.begin(), qToastcoords.end());
	}

	_numVertices = (GLsizei)vertexPosData.size();

	//GLuint vertexPositionBuffer, vertexColorBuffer;
	glGenVertexArrays(1, &_VAO); // generate array
	glBindVertexArray(_VAO); // bind array

	// Vertex positions
	GLuint positionLocation = 0;
	glGenBuffers(1, &_vertexPositionBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, _numVertices*sizeof(glm::vec4), &vertexPosData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(positionLocation); // Set location
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Vertex toastcoords
	GLuint tcLocation = 1;
	glGenBuffers(1, &_vertexToastcoordBuffer); // generate buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vertexToastcoordBuffer); // bind buffer
	glBufferData(GL_ARRAY_BUFFER, _numVertices*sizeof(glm::vec2), &vertexToastData[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(tcLocation); // Set location
	glVertexAttribPointer(tcLocation, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

void ToastSphere::cleanupOpenGLData() {
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

} // namespace planetgeometry
} // namespace openspace