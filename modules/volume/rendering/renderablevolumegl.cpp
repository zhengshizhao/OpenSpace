/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2015                                                               *
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

#include <modules/volume/rendering/renderablevolumegl.h>

#include <openspace/engine/openspaceengine.h>
#include <modules/kameleon/include/kameleonwrapper.h>
#include <openspace/util/constants.h>
#include <openspace/abuffer/abuffer.h>
#include <openspace/rendering/renderengine.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/opengl/framebufferobject.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/texture.h>

#include <algorithm>

namespace {
    const std::string _loggerCat = "RenderableVolumeGL";
    const std::string KeyVolume = "Volume";
    const std::string KeyHints = "Hints";
    const std::string KeyTransferFunction = "TransferFunction";
    const std::string KeySampler = "Sampler";
    const std::string KeyBoxScaling = "BoxScaling";
    const std::string KeyVolumeName = "VolumeName";
}

namespace openspace {

RenderableVolumeGL::RenderableVolumeGL(const ghoul::Dictionary& dictionary)
	: RenderableVolume(dictionary)
	, _volumeName("")
	, _volume(nullptr)
	, _transferFunction(nullptr)
{
	std::string name;
	bool success = dictionary.getValue(constants::scenegraphnode::keyName, name);
	assert(success);
    
	_filename = "";
	success = dictionary.getValue(KeyVolume, _filename);
	if (!success) {
		LERROR("Node '" << name << "' did not contain a valid '" <<  KeyVolume << "'");
		return;
	}
	_filename = absPath(_filename);
	if (_filename == "") {
		return;
	}

	dictionary.getValue(KeyHints, _hintsDictionary);

	_transferFunction = nullptr;
	_transferFunctionPath = "";

	success = dictionary.getValue(KeyTransferFunction, _transferFunctionPath);
	if (!success) {
		LERROR("Node '" << name << "' did not contain a valid '" << KeyTransferFunction << "'");
		return;
	}

	_transferFunctionPath = absPath(_transferFunctionPath);
	_transferFunction = new TransferFunction(_transferFunctionPath);

	KameleonWrapper kw(_filename);
	auto t = kw.getGridUnits();

	if (dictionary.hasKey(KeyBoxScaling)) {
		glm::vec4 scalingVec4(_boxScaling, _w);
		success = dictionary.getValue(KeyBoxScaling, scalingVec4);
		if (success) {
			_boxScaling = scalingVec4.xyz;
			_w = scalingVec4.w;
		}
		else {
			success = dictionary.getValue(KeyBoxScaling, _boxScaling);
			if (!success) {
				LERROR("Node '" << name << "' did not contain a valid '" <<
					KeyBoxScaling << "'");
				return;
			}
		}
	}
	else {
		// Automatic scale detection from model
		_boxScaling = kw.getModelScale();
		if (std::get<0>(t) == "R" && std::get<1>(t) == "R" && std::get<2>(t) == "R") {
			// Earth radius
			_boxScaling.x *= 6.371f;
			_boxScaling.y *= 6.371f;
			_boxScaling.z *= 6.371f;
			_w = 6;
		}
		else if (std::get<0>(t) == "m" && std::get<1>(t) == "radian" && std::get<2>(t) == "radian") {
			// For spherical coordinate systems the radius is in meter
			_w = -log10(1.0f/kw.getGridMax().x);
		}
		else {
			LWARNING("Unsupported units for automatic scale detection");
		}
	}

	_pscOffset = kw.getModelBarycenterOffset();
	if (std::get<0>(t) == "R" && std::get<1>(t) == "R" && std::get<2>(t) == "R") {
		// Earth radius
		_pscOffset[0] *= 6.371f;
		_pscOffset[1] *= 6.371f;
		_pscOffset[2] *= 6.371f;
		_pscOffset[3] = 6;
	}
	else {
		// Current spherical models no need for offset
	}
	
	dictionary.getValue(KeyVolumeName, _volumeName);
    setBoundingSphere(PowerScaledScalar::CreatePSS(glm::length(_boxScaling)*pow(10,_w)));
}

RenderableVolumeGL::~RenderableVolumeGL() {
    //OsEng.renderEngine()->aBuffer()->removeVolume(this);
    if (_volume)
	delete _volume;
    if (_transferFunction)
	delete _transferFunction;
    _volume = nullptr;
    _transferFunction = nullptr;
}

bool RenderableVolumeGL::isReady() const {
	bool ready = true;
	ready &= (_boxProgram != nullptr);
	ready &= (_volume != nullptr);
	ready &= (_transferFunction != nullptr);
	return ready; 
}

bool RenderableVolumeGL::initialize() {
    bool success = RenderableVolume::initialize();

	// @TODO fix volume and transferfunction names --jonasstrandstedt
    if(_filename != "") {
        _volume = loadVolume(_filename, _hintsDictionary);
        _volume->uploadTexture();
    }
    
    success &= isReady();

    return success;
}

bool RenderableVolumeGL::deinitialize() {
	if (_volume)
		delete _volume;
	if (_transferFunction)
		delete _transferFunction;
	_volume = nullptr;
	_transferFunction = nullptr;
    
    return RenderableVolume::deinitialize();
}

void RenderableVolumeGL::preResolve(ghoul::opengl::ProgramObject* program) {
	std::string transferFunctionName = getGlslName("transferFunction");
	std::string volumeName = getGlslName("volume");
	std::string stepSizeName = getGlslName("stepSize");

	int transferFunctionUnit = getTextureUnit(_transferFunction->getTexture());
	int volumeUnit = getTextureUnit(_volume);

	program->setUniform(transferFunctionName, transferFunctionUnit);
	program->setUniform(volumeName, volumeUnit);

	const glm::size3_t dims = _volume->dimensions();
	float maxDiag = glm::sqrt(static_cast<float>(dims.x*dims.x + dims.y*dims.y + dims.z*dims.z));
	program->setUniform(stepSizeName, static_cast<float>(glm::sqrt(3.0) / maxDiag));
}


std::string RenderableVolumeGL::getSampler(const std::string& functionName) {
	std::string transferFunctionName = getGlslName("transferFunction");
	std::string volumeName = getGlslName("volume");
	std::string stepSizeName = getGlslName("stepSize");

	std::stringstream ss;
	ss << "vec4 " << functionName << "(vec3 samplePos, vec3 dir, "
	   << "float occludingAlpha, inout float maxStepSize) {" << std::endl;

	ss << "float intensity = texture(" << volumeName << ", samplePos).x;" << std::endl;
	ss << "vec4 contribution = texture(" << transferFunctionName << ", intensity);" << std::endl;
	ss << "maxStepSize = " << stepSizeName << ";" << std::endl;
	ss << "return contribution;" << std::endl;
	ss << "}" << std::endl;

	return ss.str();
}


std::string RenderableVolumeGL::getStepSizeFunction(const std::string& functionName) {
	std::string stepSizeName = getGlslName("stepSize");

	std::stringstream ss;
	ss << "float " << functionName << "(vec3 samplePos, vec3 dir) {" << std::endl
	   << "return " << stepSizeName << ";" << std::endl
	   << "}" << std::endl;
	return ss.str();
}


std::string RenderableVolumeGL::getHeader() {
	std::string transferFunctionName = getGlslName("transferFunction");
	std::string volumeName = getGlslName("volume");
	std::string stepSizeName = getGlslName("stepSize");

	std::stringstream ss;
	ss << "uniform sampler1D " << transferFunctionName << ";" << std::endl;
	ss << "uniform sampler3D " << volumeName << ";" << std::endl;
	ss << "uniform float " << stepSizeName << ";" << std::endl;

	return ss.str();
}

std::vector<ghoul::opengl::Texture*> RenderableVolumeGL::getTextures() {
	std::vector<ghoul::opengl::Texture*> textures{_transferFunction->getTexture(), _volume};
	return textures;
}


void RenderableVolumeGL::update(const UpdateData& data) {
}

} // namespace openspace
