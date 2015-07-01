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
#include <openspace/abuffer/abuffer.h>

// ghoul includes
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/cachemanager.h>

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
    , _boxScaling(1.0, 1.0, 1.0) {}

RenderableVolume::~RenderableVolume() {}

bool RenderableVolume::initialize() {
    OsEng.configurationManager()->getValue("RaycastProgram", _boxProgram);

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

    renderIntersection(data);

    _boxProgram->deactivate();
}

glm::vec4 RenderableVolume::perspectiveToModelSpace(const RenderData& data, glm::vec4 vector) {
    glm::mat4 modelTransform = glm::mat4(1.0);
    modelTransform = glm::scale(modelTransform, _boxScaling);

    glm::mat4 invViewProjectionMatrix = glm::inverse(data.camera.viewProjectionMatrix());
    vector = invViewProjectionMatrix * vector;

    psc pscVector(vector.xyz());

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
    glm::mat3 invModelTransform = glm::mat3(glm::inverse(modelTransform));
    vector = glm::vec4(invModelTransform * vector.xyz(), vector.w);

    return vector;
}

void RenderableVolume::renderIntersection(const RenderData& data) {

    float size = 1.0;
    float nearZ = 1.0001;

    glm::vec4 tl = perspectiveToModelSpace(data, glm::vec4(-size, size, 1.0, nearZ));
    glm::vec4 bl = perspectiveToModelSpace(data, glm::vec4(-size, -size, 1.0, nearZ));
    glm::vec4 tr = perspectiveToModelSpace(data, glm::vec4(size, size, 1.0, nearZ));
    glm::vec4 br = perspectiveToModelSpace(data, glm::vec4(size, -size, 1.0, nearZ));

    const GLfloat corner_vertex_data[] = {
        //  x,     y,     z,     s,
        tl.x, tl.y, tl.z, tl.w,
        bl.x, bl.y, bl.z, bl.w,
        br.x, br.y, br.z, br.w,

        br.x, br.y, br.z, br.w,
        tr.x, tr.y, tr.z, tr.w,
        tl.x, tl.y, tl.z, tl.w,
    };

    glBindBuffer(GL_ARRAY_BUFFER, _intersectionVertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(corner_vertex_data), corner_vertex_data, GL_STATIC_DRAW);

    glBindVertexArray(_intersectionArray);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_CULL_FACE);
/*
    psc nearPlaneNormalStart = perspectiveToModelSpace(data, psc(0.0, 0.0, -1.0, 0.0));
    psc nearPlaneNormalEnd = perspectiveToModelSpace(data, psc(0.0, 0.0, 0.0, 0.0));

    glm::vec3 nearPlaneNormal = (nearPlaneNormalEnd - nearPlaneNormalStart).vec3();

    bool in[8];
    int dirSum = 0;
    for (int i = 0; i < 8; i++) {
        glm::vec3 corner(i % 2, (i / 2) % 2, i / 4) - 0.5;
        inFront[i] = glm::dot(nearPlaneNormal, corner) > 0;
        dirSum += inFront[i];
    }
    bool invserseDirection = dirSum > 4;
    int intersectionCase = invserseDirection ? 8 - dirSum : dirSum; // 0-4

    if (intersectionCase == 0) return;

    GLfloat intersection_vertex_data[18];

    glBindBuffer(GL_ARRAY_BUFFER, _intersectionVertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(intersection_vertex_data), intersection_vertex_data, GL_STATIC_DRAW);

    glBindVertexArray(_intersectionArray);
    glCullFace(GL_BACK);
    glDrawArrays(GL_TRIANGLES, 1, nVertices);
*/
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
    
ghoul::opengl::Texture* RenderableVolume::loadTransferFunction(const std::string& filepath) {

    std::string f = absPath(filepath);

    if ( ! FileSys.fileExists(f)) {
        return nullptr;
    }
    ghoul::opengl::Texture::FilterMode filtermode = ghoul::opengl::Texture::FilterMode::Linear;
    ghoul::opengl::Texture::WrappingMode wrappingmode = ghoul::opengl::Texture::WrappingMode::ClampToEdge;

    
    // check if not a txt based texture
    if ( ! hasExtension(filepath, "txt")) {
        ghoul::opengl::Texture* t = ghoul::io::TextureReader::ref().loadTexture(f);
        t->setWrapping(wrappingmode);
        return t;
    }
    
    // it is a txt based texture
    std::ifstream in;
    in.open(filepath.c_str());
    if (!in.is_open()) {
        LERROR("Could not open file " << f);
        return nullptr;
    }
    
    int width = 512;
    float lower = 0.0f;
    float upper = 1.0f;
    
    struct mappingKey {
        float position{0.0f};
        glm::vec4 color{0.0f,0.0f,0.0f,0.0f};
        
        mappingKey(float p, const glm::vec4& c): position(p), color(c){};
        mappingKey(float p): position(p), color(glm::vec4(0.0f)){};
        bool operator<(const mappingKey& rhs) {return position < rhs.position;};
    };
    
    std::vector<mappingKey> mappingKeys;
    
    auto widthValidator = [](size_t in) { if(in > 0) return in; return static_cast<size_t>(1); };
    auto upperLowerValidator = [](float in) { return glm::clamp(in, 0.0f, 1.0f); };
    auto intensityValidator = [](float in) { return glm::clamp(in, 0.0f, 1.0f); };
    
    std::string line;
    while (std::getline(in, line)) {
        
        glm::vec4 rgba = glm::vec4(0.0f);
        // tokenize the line
        std::istringstream iss(line);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},std::istream_iterator<std::string>{}};
        
        size_t tokenSize =tokens.size();
        if (tokenSize > 0) {
            std::string key = tokens.at(0);
            if(key == "width" && tokenSize == 2) {
                width = static_cast<int>(stringToNumber<size_t>(tokens.at(1), widthValidator));
            } else if(key == "lower" && tokenSize == 2) {
                lower = stringToNumber<float>(tokens.at(1), upperLowerValidator);
            } else if(key == "upper" && tokenSize == 2) {
                upper = stringToNumber<float>(tokens.at(1),upperLowerValidator);
            } else if(key == "mappingkey" && tokenSize == 6) {
                float intensity = stringToNumber<float>(tokens.at(1), intensityValidator);
                for(int i = 0; i < 4; ++i)
                    rgba[i] = stringToNumber<float>(tokens.at(i+2));

                mappingKeys.push_back({intensity, rgba});
            }
        }
    }
    in.close();
    

    if (mappingKeys.size() < 1) {
        return nullptr;
    }

    // for(auto key: mappingKeys) {
    //     glm::vec4 rgba = key.color;
    //    LDEBUG("i: " << key.position << ", rgba: (" << rgba[0] << ", " << rgba[1] << ", " << rgba[2] << ", " << rgba[3] << ")");
    // }
    // LDEBUG("insert....");
    
    if (mappingKeys.front().position > lower){
        mappingKeys.insert(mappingKeys.begin(), {lower,mappingKeys.front().color});
    }

    if (mappingKeys.back().position < upper){
        mappingKeys.push_back({upper,mappingKeys.back().color});
    }
    
    
    // for(auto key: mappingKeys) {
    //     glm::vec4 rgba = key.color;
    //    LDEBUG("i: " << key.position << ", rgba: (" << rgba[0] << ", " << rgba[1] << ", " << rgba[2] << ", " << rgba[3] << ")");
    // }
    
    // allocate new float array with zeros
    float* transferFunction = new float[width*4]();
    for (int i = 0; i < 4*width; ++i) {
        transferFunction[i] = 0.0f;
    }
    
    size_t lowerIndex = static_cast<size_t>(floorf(lower*static_cast<float>(width-1)));
    size_t upperIndex = static_cast<size_t>(floorf(upper*static_cast<float>(width-1)));
    
//    LDEBUG("width: " << width);
//    LDEBUG("lower: " << lower);
//    LDEBUG("upper: " << upper);
//    LDEBUG("lowerIndex: " << lowerIndex);
//    LDEBUG("upperIndex: " << upperIndex);
    
    
    auto prevKey = mappingKeys.begin();
    auto currentKey = prevKey + 1;
    auto lastKey = mappingKeys.end() -1;
    
    for (size_t i=lowerIndex; i<=upperIndex; i++) {
        
        float fpos = static_cast<float>(i)/static_cast<float>(width-1);
        
        if (fpos > (*currentKey).position) {
            prevKey = currentKey;
            currentKey++;
            if (currentKey == mappingKeys.end()) {
                currentKey = lastKey;
            }
        }
        
        float dist = fpos-(*prevKey).position;
        float weight = dist/((*currentKey).position-(*prevKey).position);
        
        //LDEBUG("fpos: " << fpos);
        //LDEBUG("(*prevKey).position: " << (*prevKey).position);
        //LDEBUG("(*currentKey).position: " << (*currentKey).position);
        //LDEBUG("weight: " << weight);
        
        for (size_t channel=0; channel<4; ++channel) {
            size_t position = 4*i + channel;
            // Interpolate linearly between prev and next mapping key
            
            //LDEBUG("i: " << i);
            //LDEBUG("position: " << position);
            //LDEBUG("(*prevKey).first " << (*prevKey).first);
            //LDEBUG("(*currentKey).first " << (*currentKey).first);
            //LDEBUG("dist: " << dist);
            //LDEBUG("weight: " << weight);
            float value =
            ((*prevKey).color[channel]*(1.f-weight) + (*currentKey).color[channel]*weight)/255.f;
            transferFunction[position] = value;
            
            //LDEBUG("["<< position <<"] " << value);
            
        }
       // LDEBUG(std::fixed << std::setw(10) << std::left << std::setprecision(8) << weight << ", (" <<
       //        std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+0] << ", " <<
       //        std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+1] << ", " <<
       //        std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+2] << ", " <<
       //        std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+3] << ")");
    }

    // for (int i = 0; i <= width; ++i) {

    //    LDEBUG(std::fixed  << "(" <<
    //           std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+0] << ", " <<
    //           std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+1] << ", " <<
    //           std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+2] << ", " <<
    //           std::setw(10) << std::left << std::setprecision(8) << transferFunction[4*i+3] << ")");
    // }

    return new ghoul::opengl::Texture(transferFunction,
    		glm::size3_t(width,1,1),ghoul::opengl::Texture::Format::RGBA,
    		GL_RGBA, GL_FLOAT,filtermode, wrappingmode);
}

} // namespace openspace
