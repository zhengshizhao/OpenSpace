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

#ifndef __ABUFFER_H__
#define __ABUFFER_H__

#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/glm.h>

#include <string>
#include <vector>
#include <set>
#include <map>

namespace ghoul {
    namespace filesystem {
        class File;
    }
	namespace opengl {
        class ProgramObject;
		class Texture;
	}
}

namespace openspace {

    class ABufferVolume;

class ABuffer {
public:

	struct fragmentData {
		GLfloat _position[3];
		GLfloat _color[4];
		GLfloat _padding;
	};

	static const int MAX_LAYERS = 32;

	ABuffer();
	virtual ~ABuffer();
	virtual void resolve();
    virtual bool initialize() = 0;
	virtual bool reinitialize();
	virtual void resetBindings();

	void registerGlslHelpers(std::string helpers);
	int addVolume(ABufferVolume* volume);
	//void removeVolume(ABufferVolume* volume);
	std::string getGlslName(ABufferVolume* volume, const std::string& key);
	int getTextureUnit(ghoul::opengl::Texture* texture);

	void invalidateABuffer();
    
    virtual void clear() = 0;
    virtual void preRender() = 0;
	virtual void postRender() = 0;

	virtual std::vector<fragmentData> pixelData() = 0;

protected:
	virtual bool reinitializeInternal() = 0;

	bool initializeABuffer();

	void generateShaderSource();
	bool updateShader();

	void generateHelpers();
	void generateHeaders();
	void generateStepSizeCalls();
	void generateSamplerCalls();
	void generateStepSizeFunctions();
	void generateSamplers();

	unsigned int _width, _height, _totalPixels;

private:

	void updateDimensions();

	GLuint _screenQuad;

	bool _validShader;
	ghoul::opengl::ProgramObject* _resolveShader;

	std::vector<std::pair<std::string,ghoul::opengl::Texture*> > _volumes;
	std::vector<std::pair<std::string,ghoul::opengl::Texture*> > _transferFunctions;

	std::set<ABufferVolume*> _aBufferVolumes;
	std::map<ABufferVolume*, std::map<std::string, std::string> > _glslDictionary;

	std::vector<ghoul::filesystem::File*> _samplerFiles;
	std::vector<std::string> _glslHelpers;
	std::vector<std::string> _samplers;
	std::string generateGlslName(const std::string& name);
	std::map<ghoul::opengl::Texture*, int> _textureUnits;
	int nextGlslNameId = 0;
	int nextId = 1;
	float _volumeStepFactor;

};		// ABuffer
}		// openspace

#endif 	// __ABUFFER_H__
