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

// open space includes
#include <openspace/rendering/planets/renderableplanet.h>
#include <openspace/util/constants.h>
#include <openspace/util/time.h>
#include <openspace/util/spicemanager.h>
#include <openspace/rendering/planets/planetgeometry.h>
#include <openspace/engine/openspaceengine.h>

#include <ghoul/opengl/programobject.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/assert.h>

#include <sgct.h>

namespace {
const std::string _loggerCat = "RenderablePlanet";
}

namespace openspace {

RenderablePlanet::RenderablePlanet(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
	, _colorTexturePath("colorTexture", "Color Texture")
    , _programObject(nullptr)
    , _geometry(nullptr)
{
	std::string name;
	bool success = dictionary.getValue(constants::scenegraphnode::keyName, name);
	ghoul_assert(success,
            "RenderablePlanet need the '" <<constants::scenegraphnode::keyName<<"' be specified");

    std::string path;
    success = dictionary.getValue(constants::scenegraph::keyPathModule, path);
    ghoul_assert(success,
            "RenderablePlanet need the '"<<constants::scenegraph::keyPathModule<<"' be specified");

    ghoul::Dictionary geometryDictionary;
    success = dictionary.getValue(
		constants::renderableplanet::keyGeometry, geometryDictionary);
	if (success) {
		geometryDictionary.setValue(constants::scenegraphnode::keyName, name);
        geometryDictionary.setValue(constants::scenegraph::keyPathModule, path);
        _geometry = planetgeometry::PlanetGeometry::createFromDictionary(geometryDictionary);
	}

	dictionary.getValue(constants::renderableplanet::keyFrame, _target);

    // TODO: textures need to be replaced by a good system similar to the geometry as soon
    // as the requirements are fixed (ab)
    std::string texturePath = "";
	success = dictionary.getValue("Textures.Color", texturePath);
	if (success)
        _colorTexturePath = path + "/" + texturePath;

	addPropertySubOwner(_geometry);

	addProperty(_colorTexturePath);
    _colorTexturePath.onChange(std::bind(&planetgeometry::PlanetGeometry::loadTexture, _geometry));

	// Temporary hack to support the two different shaders for 
	// ToastSphere and SimpleSphere (hc)
	std::string geometryType;
	geometryDictionary.getValue(constants::planetgeometry::keyType, geometryType);
	if (geometryType == "ToastSphere" && _programObject == nullptr) {
		OsEng.ref().configurationManager().getValue("ToastPlanetProgram", _programObject);
	} else if (_programObject == nullptr) {
		OsEng.ref().configurationManager().getValue("pscShader", _programObject);
	}
}

RenderablePlanet::~RenderablePlanet() {
    deinitialize();
}

bool RenderablePlanet::initialize() {
    //if (_programObject == nullptr)
    //    OsEng.ref().configurationManager().getValue("pscShader", _programObject);
	
    _geometry->initialize(this);

    return isReady();
}

bool RenderablePlanet::deinitialize() {
    if(_geometry) {
        _geometry->deinitialize();
        delete _geometry;
    }

    _geometry = nullptr;
    return true;
}

bool RenderablePlanet::isReady() const {
    bool ready = true;
    ready &= (_programObject != nullptr);
    ready &= (_geometry != nullptr);
	return ready;
}

void RenderablePlanet::render(const RenderData& data) {
    // activate shader
    _programObject->activate();

    // scale the planet to appropriate size since the planet is a unit sphere
    glm::mat4 transform = glm::mat4(1);
	
	//earth needs to be rotated for that to work.
	glm::mat4 rot = glm::rotate(transform, 90.f, glm::vec3(1, 0, 0));
		
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 3; j++){
			transform[i][j] = _stateMatrix[i][j];
		}
	}
	transform = transform* rot;
	
	//glm::mat4 modelview = data.camera.viewMatrix()*data.camera.modelMatrix();
	//glm::vec3 camSpaceEye = (-(modelview*data.position.vec4())).xyz;

    // setup the data to the shader
	//_programObject->setUniform("camdir", camSpaceEye);
	_programObject->setUniform("ViewProjection", data.camera.viewProjectionMatrix());
	_programObject->setUniform("ModelTransform", transform);
	setPscUniforms(_programObject, &data.camera, data.position);

    // bind texture and render
	_geometry->bindTexture(_programObject);
    _geometry->render();

    // disable shader
    _programObject->deactivate();
}

void RenderablePlanet::update(const UpdateData& data){
	// set spice-orientation in accordance to timestamp
	openspace::SpiceManager::ref().getPositionTransformMatrix(_target, "GALACTIC", data.time, _stateMatrix);
}

}  // namespace openspace
