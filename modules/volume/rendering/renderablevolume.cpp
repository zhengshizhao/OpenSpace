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

// open space includes
#include <openspace/engine/configurationmanager.h>
#include <modules/volume/rendering/renderablevolume.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <modules/kameleon/include/kameleonwrapper.h>
#include <openspace/util/constants.h>
#include <openspace/util/progressbar.h>
#include <openspace/util/spicemanager.h>
#include <openspace/abuffer/abuffer.h>

// ghoul includes
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/cachemanager.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <type_traits>

namespace {
    const std::string _loggerCat = "RenderableVolume";
    
    bool hasExtension (std::string const &filepath, std::string const &extension)
    {
        std::string ending = "." + extension;
        if (filepath.length() > ending.length()) {
            return (0 == filepath.compare (filepath.length() - ending.length(), ending.length(), ending));
        } else {
            return false;
        }
    }
    
    template<typename T>
    T stringToNumber(const std::string& number, std::function<T(T)> f = nullptr) {
        static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                      "Must be integral or floating point");
        std::stringstream ss;
        T templateNumber = {0};
        ss << number;
        ss >> templateNumber;
        if( ! f)
            return templateNumber;

        return f(templateNumber);

    }
}

namespace openspace {

RenderableVolume::RenderableVolume(const ghoul::Dictionary& dictionary) : Renderable(dictionary)
    , _w(0.f)
    , _boxArray(0)
    , _vertexPositionBuffer(0)
    , _boxProgram(nullptr)
    , _nearPlaneProgram(nullptr)
    , _boxScaling(1.0, 1.0, 1.0)
    , _alphaCoefficient("alphaCoefficient", "Alpha coefficient", 10.0f, 0.f, 32.f) {
    addProperty(_alphaCoefficient);
}


RenderableVolume::~RenderableVolume() {}

bool RenderableVolume::initialize() {
    OsEng.configurationManager()->getValue("RaycastProgram", _boxProgram);
    OsEng.configurationManager()->getValue("NearPlaneProgram", _nearPlaneProgram);



    // ============================
    //      GEOMETRY (box)
    // ============================
    const GLfloat size = 0.5f;
    const GLfloat vertex_data[] = {
        //  x,     y,     z,     s,
        -size, -size,  size,  _w,
         size,  size,  size,  _w,
        -size,  size,  size,  _w,
        -size, -size,  size,  _w,
         size, -size,  size,  _w,
         size,  size,  size,  _w,

        -size, -size, -size,  _w,
         size,  size, -size,  _w,
        -size,  size, -size,  _w,
        -size, -size, -size,  _w,
         size, -size, -size,  _w,
         size,  size, -size,  _w,

         size, -size, -size,  _w,
         size,  size,  size,  _w,
         size, -size,  size,  _w,
         size, -size, -size,  _w,
         size,  size, -size,  _w,
         size,  size,  size,  _w,

        -size, -size, -size,  _w,
        -size,  size,  size,  _w,
        -size, -size,  size,  _w,
        -size, -size, -size,  _w,
        -size,  size, -size,  _w,
        -size,  size,  size,  _w,

        -size,  size, -size,  _w,
         size,  size,  size,  _w,
        -size,  size,  size,  _w,
        -size,  size, -size,  _w,
         size,  size, -size,  _w,
         size,  size,  size,  _w,

        -size, -size, -size,  _w,
         size, -size,  size,  _w,
        -size, -size,  size,  _w,
        -size, -size, -size,  _w,
         size, -size, -size,  _w,
         size, -size,  size,  _w,
    };

    glGenVertexArrays(1, &_boxArray); // generate array
    glBindVertexArray(_boxArray); // bind array
    glGenBuffers(1, &_vertexPositionBuffer); // generate buffer
    glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &_intersectionArray); // generate array
    glBindVertexArray(_intersectionArray); // bind array
    glGenBuffers(1, &_intersectionVertexPositionBuffer); // generate buffer
    glBindBuffer(GL_ARRAY_BUFFER, _intersectionVertexPositionBuffer); // bind buffer
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // add the volume and get the ID
    _id = OsEng.renderEngine()->aBuffer()->addVolume(this);

    return true;
}

bool RenderableVolume::deinitialize() {
    glDeleteVertexArrays(1, &_boxArray);
    glGenBuffers(1, &_vertexPositionBuffer);

    glDeleteVertexArrays(1, &_intersectionArray);
    glGenBuffers(1, &_intersectionVertexPositionBuffer);

    return true;
}

    void RenderableVolume::update(const UpdateData& data){
	/*if (_modelName == "ENLIL") {

	    psc earthPosition;
	    glm::dmat3 sunRotation;
	    double lt = 0;

	    openspace::SpiceManager::ref().getTargetPosition("EARTH", "SUN", "IAU_SUN", "NONE", data.time, earthPosition, lt);
	    openspace::SpiceManager::ref().getPositionTransformMatrix("GALACTIC", "IAU_SUN", data.time, sunRotation);

	    glm::mat4 rot;

	    glm::vec3 earthPos = glm::normalize(earthPosition.vec4().xyz());

	    glm::dvec3 sunNorthPole = sunRotation*glm::dvec3(0.0, 0.0, 1.0);
	    glm::dvec3 sunEarth = sunRotation*glm::dvec3(earthPosition.vec3());

		//std::cout << glm::dot(glm::normalize(sunNorthPole), glm::normalize(sunEarth)) << std::endl;
	}*/
}

void RenderableVolume::preResolve(ghoul::opengl::ProgramObject* program) {
    std::stringstream ss;
    ss << "alphaCoefficient_" << getId();
    program->setUniform(ss.str(), _alphaCoefficient);
};


void RenderableVolume::render(const RenderData& data) {
    glm::mat4 transform = glm::mat4(1.0);
    transform = glm::scale(transform, _boxScaling);

    // fetch data
    psc currentPosition = data.position;
    currentPosition += _pscOffset; // Move box to model barycenter

    _boxProgram->activate();
    _boxProgram->setUniform("volumeType", _id);
    _boxProgram->setUniform("viewProjection", data.camera.viewProjectionMatrix());
    _boxProgram->setUniform("modelTransform", transform);
    setPscUniforms(_boxProgram, &data.camera, currentPosition);

    glDisable(GL_CULL_FACE);
    glBindVertexArray(_boxArray);
    glDrawArrays(GL_TRIANGLES, 0, 6*6);
    glEnable(GL_CULL_FACE);
    _boxProgram->deactivate();

    _nearPlaneProgram->activate();
    _nearPlaneProgram->setUniform("volumeType", _id);
    _nearPlaneProgram->setUniform("viewProjection", data.camera.viewProjectionMatrix());
    _nearPlaneProgram->setUniform("modelTransform", transform);
    _nearPlaneProgram->setUniform("campos", data.camera.position().vec4());
    _nearPlaneProgram->setUniform("objpos", currentPosition.vec4());
    _nearPlaneProgram->setUniform("camrot", data.camera.viewRotationMatrix());
    _nearPlaneProgram->setUniform("scaling", data.camera.scaling());
    renderIntersection(data);
    _nearPlaneProgram->deactivate();

}

glm::vec3 RenderableVolume::perspectiveToCubeSpace(const RenderData& data, glm::vec4 vector) {
    glm::mat4 invViewProjectionMatrix = glm::inverse(data.camera.viewProjectionMatrix());
    vector = invViewProjectionMatrix * vector;

    glm::vec2 cameraScaling = data.camera.scaling();
    psc pscVector(glm::vec4(vector.xyz() / cameraScaling.x, -cameraScaling.y));

    glm::mat3 invCamRot = glm::mat3(glm::inverse(data.camera.viewRotationMatrix()));
    vector = pscVector.vec4();
    // vector is a psc
    vector = glm::vec4(invCamRot * vector.xyz(), vector.w);

    pscVector = psc(vector);
    pscVector += data.camera.position();

    psc currentPosition = data.position;
    currentPosition += _pscOffset; // Move box to model barycenter
    pscVector -= currentPosition;

    vector = pscVector.vec4();

    glm::mat4 modelTransform = glm::mat4(1.0);
    modelTransform = glm::scale(modelTransform, _boxScaling);
    glm::mat3 invModelTransform = glm::mat3(glm::inverse(modelTransform));
    vector = glm::vec4(invModelTransform * vector.xyz(), vector.w);
    vector.w -= _w;

    pscVector = psc(vector);

    return pscVector.vec3();
}

void RenderableVolume::renderIntersection(const RenderData& data) {
    // Draw only intersecting polygon
    int errorI = 0;

    const int cornersInLines[24] = {
        0,1,
        1,5,
        5,4,
        4,0,

        2,3,
        3,7,
        7,6,
        6,2,

        0,2,
        1,3,
        5,7,
        4,6
    };

    // Get powerscaled coordinates of normal ends
    float nearZ = data.camera.nearPlane();
    float nearW = nearZ;

    glm::vec3 nearPlaneP0 = perspectiveToCubeSpace(data, glm::vec4(0.0, 0.0, nearZ, nearW));
    glm::vec3 nearPlaneP1 = perspectiveToCubeSpace(data, glm::vec4(1.0, 0.0, nearZ, nearW));
    glm::vec3 nearPlaneP2 = perspectiveToCubeSpace(data, glm::vec4(0.0, 1.0, nearZ, nearW));

    glm::vec3 nearPlaneNormal = glm::normalize(glm::cross(nearPlaneP1 - nearPlaneP0, nearPlaneP2 - nearPlaneP0));
    float nearPlaneDistance = glm::dot(nearPlaneP0, nearPlaneNormal);

    glm::vec3 intersections[12];
    int nIntersections = 0;

    for (int i = 0; i < 12; i++) {
        int iCorner0 = cornersInLines[i * 2];
        int iCorner1 = cornersInLines[i * 2 + 1];

        glm::vec3 corner0 = glm::vec3(iCorner0 % 2, (iCorner0 / 2) % 2, iCorner0 / 4) - glm::vec3(0.5);
        glm::vec3 corner1 = glm::vec3(iCorner1 % 2, (iCorner1 / 2) % 2, iCorner1 / 4) - glm::vec3(0.5);

        glm::vec3 line = corner1 - corner0;

        float t = (nearPlaneDistance - glm::dot(corner0, nearPlaneNormal)) / glm::dot(line, nearPlaneNormal);
        if (t >= 0.0 && t <= 1.0) {
            intersections[nIntersections++] = corner0 + t * line;
        }
    }

    if (nIntersections <3) return; // Gotta love intersections

    std::vector< std::pair<int, float> > angles(nIntersections-1);

    glm::vec3 vector1 = glm::normalize(intersections[1] - intersections[0]);
    angles[0] = std::pair<int, float>(1, 0.0);

    for (int i = 2; i < nIntersections; i++) {
        glm::vec3 vectorI = glm::normalize(intersections[i] - intersections[0]);
        float sinA = glm::dot(glm::cross(vector1, vectorI), nearPlaneNormal);
        float cosA = glm::dot(vector1, vectorI);
        angles[i-1] = std::pair<int, float>(i, glm::sign(sinA) * (1.0 - cosA));
    }

    std::sort(angles.begin(), angles.end(),
        [](const std::pair<int, float>& a, const std::pair<int, float>& b) -> bool {
            return a.second > b.second;
        }
    );

    std::vector<float> vertices;
    vertices.push_back(intersections[0].x);
    vertices.push_back(intersections[0].y);
    vertices.push_back(intersections[0].z);
    vertices.push_back(_w);
    for (int i = 0; i < nIntersections - 1; i++) {
        int j = angles[i].first;
        vertices.push_back(intersections[j].x);
        vertices.push_back(intersections[j].y);
        vertices.push_back(intersections[j].z);
        vertices.push_back(_w);
    }
    glBindBuffer(GL_ARRAY_BUFFER, _intersectionVertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * nIntersections, vertices.data(), GL_STATIC_DRAW);

    glBindVertexArray(_intersectionArray);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLE_FAN, 0, nIntersections);
    glEnable(GL_CULL_FACE);
}

ghoul::opengl::Texture* RenderableVolume::loadVolume(
    const std::string& filepath, 
    const ghoul::Dictionary& hintsDictionary) {

	if( ! FileSys.fileExists(filepath)) {
		LWARNING("Could not load volume, could not find '" << filepath << "'");
		return nullptr;
	}

	if(hasExtension(filepath, "raw")) {
		ghoul::RawVolumeReader::ReadHints hints = readHints(hintsDictionary);
		ghoul::RawVolumeReader rawReader(hints);
		return rawReader.read(filepath);
	} else if(hasExtension(filepath, "cdf")) {

        ghoul::opengl::Texture::FilterMode filtermode = ghoul::opengl::Texture::FilterMode::Linear;
        ghoul::opengl::Texture::WrappingMode wrappingmode = ghoul::opengl::Texture::WrappingMode::ClampToEdge;

        glm::size3_t dimensions(1,1,1);
        double tempValue;
        if (hintsDictionary.hasKey("Dimensions.1") && 
            hintsDictionary.getValue("Dimensions.1", tempValue)) {
            int intVal = static_cast<int>(tempValue);
            if(intVal > 0)
                dimensions[0] = intVal;
        }
        if (hintsDictionary.hasKey("Dimensions.2") && 
            hintsDictionary.getValue("Dimensions.2", tempValue)) {
            int intVal = static_cast<int>(tempValue);
            if(intVal > 0)
                dimensions[1] = intVal;
        }
        if (hintsDictionary.hasKey("Dimensions.3") && 
            hintsDictionary.getValue("Dimensions.3", tempValue)) {
            int intVal = static_cast<int>(tempValue);
            if(intVal > 0)
                dimensions[2] = intVal;
        }

        std::string variableCacheString = "";
        if (hintsDictionary.hasKey("Variable")) {
            hintsDictionary.getValue("Variable", variableCacheString);
        } else if(hintsDictionary.hasKey("Variables")) {  
            std::string xVariable, yVariable, zVariable;
            bool xVar, yVar, zVar;
            xVar = hintsDictionary.getValue("Variables.1", xVariable);
            yVar = hintsDictionary.getValue("Variables.2", yVariable);
            zVar = hintsDictionary.getValue("Variables.3", zVariable);
            if (xVar && yVar && zVar) {
                variableCacheString = xVariable + "." + yVariable + "." + zVariable;
            }
        }

        bool cache = false;
        hintsDictionary.hasKey("Cache");
        if (hintsDictionary.hasKey("Cache"))
            hintsDictionary.getValue("Cache", cache);

        std::stringstream ss;
        ss << "." << dimensions[0] << "x" << dimensions[1] << "x" << dimensions[2] << "." << "." << variableCacheString << ".cache";

		std::string cachepath; // = filepath + ss.str();
        ghoul::filesystem::File ghlFile(filepath);
		FileSys.cacheManager()->getCachedFile(ghlFile.baseName(), ss.str(), cachepath, true);
		if (cache && FileSys.fileExists(cachepath)) {
           
#define VOLUME_LOAD_PROGRESSBAR
            std::ifstream file(cachepath, std::ios::binary | std::ios::in);
			if (file.is_open()) {
				size_t length = dimensions[0] * dimensions[1] * dimensions[2];
				float* data = new float[length];
#ifdef VOLUME_LOAD_PROGRESSBAR
				LINFO("Loading cache: " << cachepath);
				ProgressBar pb(static_cast<int>(dimensions[2]));
				for (size_t i = 0; i < dimensions[2]; ++i) {
					size_t offset = length / dimensions[2];
					std::streamsize offsetsize = sizeof(float)*offset;
					file.read(reinterpret_cast<char*>(data + offset * i), offsetsize);
					pb.print(static_cast<int>(i));
				}
#else
				file.read(reinterpret_cast<char*>(data), sizeof(float)*length);
#endif
                file.close();
                return new ghoul::opengl::Texture(data, dimensions, ghoul::opengl::Texture::Format::Red, GL_RED, GL_FLOAT, filtermode, wrappingmode);
            } else {
                return nullptr;
            }
        }

		KameleonWrapper kw(filepath);
		std::string variableString;
		if (hintsDictionary.hasKey("Variable") && hintsDictionary.getValue("Variable", variableString)) {
			float* data = kw.getUniformSampledValues(variableString, dimensions);
            if(cache) {
                std::ofstream file(cachepath, std::ios::binary | std::ios::out);
                if (file.is_open()) {
                    size_t length = dimensions[0] * dimensions[1] * dimensions[2];
                    file.write(reinterpret_cast<const char*>(data), sizeof(float)*length);
                    file.close();
                }
            }
        	return new ghoul::opengl::Texture(data, dimensions, ghoul::opengl::Texture::Format::Red, GL_RED, GL_FLOAT, filtermode, wrappingmode);
		} else if (hintsDictionary.hasKey("Variables")) {
			std::string xVariable, yVariable, zVariable;
			bool xVar, yVar, zVar;
			xVar = hintsDictionary.getValue("Variables.1", xVariable);
			yVar = hintsDictionary.getValue("Variables.2", yVariable);
			zVar = hintsDictionary.getValue("Variables.3", zVariable);

			if (!xVar || !yVar || !zVar) {
				LERROR("Error reading variables! Must be 3 and must exist in CDF data");
			} else {

				float* data = kw.getUniformSampledVectorValues(xVariable, yVariable, zVariable, dimensions);
                if(cache) {
                    //FILE* file = fopen (cachepath.c_str(), "wb");
					std::ofstream file(cachepath, std::ios::in | std::ios::binary);
					size_t length = dimensions[0] * dimensions[1] * dimensions[2];
					file.write(reinterpret_cast<char*>(data), sizeof(float)*length);
                    file.close();
                }

				return new ghoul::opengl::Texture(data, dimensions, ghoul::opengl::Texture::Format::RGBA, GL_RGBA, GL_FLOAT, filtermode, wrappingmode);
			}

		} else {
			LWARNING("Hints does not specify a 'Variable' or 'Variables'");
		}
	} else {
		LWARNING("No valid file extension.");
	}
	return nullptr;
}

glm::vec3 RenderableVolume::getVolumeOffset(
		const std::string& filepath,
		const ghoul::Dictionary& hintsDictionary) {

	KameleonWrapper kw(filepath);
	return kw.getModelBarycenterOffset();
}

ghoul::RawVolumeReader::ReadHints RenderableVolume::readHints(const ghoul::Dictionary& dictionary) {
    ghoul::RawVolumeReader::ReadHints hints;
    hints._dimensions = glm::ivec3(1, 1, 1);
    hints._format = ghoul::opengl::Texture::Format::Red;
    hints._internalFormat = GL_R8;
    
    // parse hints
    double tempValue;
    if (dictionary.hasKey("Dimensions.1") && dictionary.getValue("Dimensions.1", tempValue)) {
        int intVal = static_cast<int>(tempValue);
        if(intVal > 0)
            hints._dimensions[0] = intVal;
    }
    if (dictionary.hasKey("Dimensions.2") && dictionary.getValue("Dimensions.2", tempValue)) {
        int intVal = static_cast<int>(tempValue);
        if(intVal > 0)
            hints._dimensions[1] = intVal;
    }
    if (dictionary.hasKey("Dimensions.3") && dictionary.getValue("Dimensions.3", tempValue)) {
        int intVal = static_cast<int>(tempValue);
        if(intVal > 0)
            hints._dimensions[2] = intVal;
    }
    
    std::string format;
    if (dictionary.hasKey("Format") && dictionary.getValue("Format", format)) {
        if(format == "RED") {
            hints._format = ghoul::opengl::Texture::Format::Red;
        } else if(format == "RG") {
            hints._format = ghoul::opengl::Texture::Format::RG;
        } else if(format == "RGB") {
            hints._format = ghoul::opengl::Texture::Format::RGB;
        } else if(format == "RGBA") {
            hints._format = ghoul::opengl::Texture::Format::RGBA;
        }
    }
    
    format = "";
    if (dictionary.hasKey("InternalFormat") && dictionary.getValue("InternalFormat", format)) {
        if(format == "R8") {
            hints._internalFormat = GL_R8;
        } else if(format == "RG8") {
            hints._internalFormat = GL_RG8;
        } else if(format == "RGB8") {
            hints._internalFormat = GL_RGB8;
        } else if(format == "RGBA8") {
            hints._internalFormat = GL_RGB8;
        } else if(format == "R32F") {
            hints._internalFormat = GL_R32F;
        } else if(format == "RG32F") {
            hints._internalFormat = GL_RG32F;
        } else if(format == "RGB32F") {
            hints._internalFormat = GL_RGB32F;
        } else if(format == "RGBA32F") {
            hints._internalFormat = GL_RGB32F;
        }
    }
    return hints;
}
    

} // namespace openspace
