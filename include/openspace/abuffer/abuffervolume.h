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

#ifndef __ABUFFERVOLUME_H__
#define __ABUFFERVOLUME_H__

#include <string>
#include <vector>

namespace ghoul {
    namespace opengl {
	class Texture;
	class ProgramObject;
    }
}

namespace openspace {

class ABufferVolume {
public:
    ABufferVolume() : _id(-1) {};
	virtual ~ABufferVolume() {};
	virtual void preResolve(ghoul::opengl::ProgramObject* program) {};
	/*
	 * Return a glsl function source, with the signature
	 * void functionName(vec3 samplePos, vec3 dir, float occludingAlpha, inout float maxStepSize)
	 */
	virtual std::string getSampler(const std::string& functionName) { return ""; };

	/*
	 * Return a glsl function source, with the signature
	 * float functionName(vec3 samplePos, vec3 dir)
	 */
	virtual std::string getStepSizeFunction(const std::string& functionName) { return ""; };
	virtual std::string getHeader() { return ""; };
	virtual std::vector<ghoul::opengl::Texture*> getTextures() { return std::vector<ghoul::opengl::Texture*>(); };
    virtual std::vector<int> getBuffers() { return std::vector<int>(); };
protected:
	std::string getGlslName(const std::string &key);
	int getTextureUnit(ghoul::opengl::Texture* texture);
    int getSsboBinding(int ssboId);

    int _id;
};          // ABufferVolume

}               // openspace

#endif  // __ABUFFER_H__
