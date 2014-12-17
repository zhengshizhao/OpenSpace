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

#include <openspace/rendering/planets/simplespheregeometry.h>
#include <openspace/util/constants.h>
#include <openspace/util/powerscaledsphere.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/io/texture/texturereader.h>

namespace {
    const std::string _loggerCat = "SimpleSphereGeometry";
}

namespace openspace {

namespace constants {
namespace simplespheregeometry {
const std::string keyRadius = "Radius";
const std::string keySegments = "Segments";
}  // namespace simplespheregeometry
}

namespace planetgeometry {

SimpleSphereGeometry::SimpleSphereGeometry(const ghoul::Dictionary& dictionary)
    : PlanetGeometry()
    , _radius("radius", "Radius", glm::vec2(1.f, 0.f), glm::vec2(-10.f, -20.f),
              glm::vec2(10.f, 20.f))
    , _segments("segments", "Segments", 20, 1, 50)
    , _planet(nullptr)
	, _texture(nullptr)
{
	using constants::scenegraphnode::keyName;
	using constants::simplespheregeometry::keyRadius;
	using constants::simplespheregeometry::keySegments;

	// The name is passed down from the SceneGraphNode
    std::string name;
    bool success = dictionary.getValue(keyName, name);
	assert(success);

    glm::vec2 radius;
    success = dictionary.getValue(keyRadius, radius);
	if (!success) {
        LERROR("SimpleSphereGeometry of '" << name << "' did not provide a key '"
                                           << keyRadius << "'");
	}
	else
		_radius = radius;

    double segments;
    success = dictionary.getValue(keySegments, segments);
	if (!success) {
        LERROR("SimpleSphereGeometry of '" << name << "' did not provide a key '"
                                           << keySegments << "'");
	}
	else
		_segments = static_cast<int>(segments);

    addProperty(_radius);
    _radius.onChange(std::bind(&SimpleSphereGeometry::createSphere, this));
    addProperty(_segments);
    _segments.onChange(std::bind(&SimpleSphereGeometry::createSphere, this));
}

SimpleSphereGeometry::~SimpleSphereGeometry()
{
}

bool SimpleSphereGeometry::initialize(RenderablePlanet* parent)
{
    bool success = PlanetGeometry::initialize(parent);
    createSphere();
	loadTexture();
    return success;
}

void SimpleSphereGeometry::deinitialize()
{
	if (_planet)
	    delete _planet;
    _planet = nullptr;
}

void SimpleSphereGeometry::bindTexture(ghoul::opengl::ProgramObject* programObject) {
	ghoul::opengl::TextureUnit unit;
	unit.activate();
	_texture->bind();
	programObject->setUniform("texture1", unit);
}

void SimpleSphereGeometry::loadTexture() {
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

void SimpleSphereGeometry::render()
{
    _planet->render();
}

void SimpleSphereGeometry::createSphere()
{
    //create the power scaled scalar

    PowerScaledScalar planetSize(_radius);
    _parent->setBoundingSphere(planetSize);

    if(_planet)
        delete _planet;

    _planet = new PowerScaledSphere(planetSize, _segments);
    _planet->initialize();
}

}  // namespace planetgeometry
}  // namespace openspace
