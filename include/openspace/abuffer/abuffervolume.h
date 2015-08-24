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
     * Return a path to a file with all the header definitions
     * (uniforms, functions etc) required to render this volume.
     *
     * The shader preprocessor will have acceess to the #{id} variable.
     *
     * The header should define the following two functions:
     *   void sampler#{id}(vec3 samplePos, vec3 dir, float occludingAlpha, inout float maxStepSize)
     *   float stepSize#{id}(vec3 samplePos, vec3 dir)
     */
    virtual std::string getHeaderPath() = 0;

    /*
     * Return a path to a glsl file that should only be included once
     * regardless of how many volumes say they require the file.
     * Ideal for helper functions.
     */
    virtual std::string getHelperPath() = 0;

    /**
     * Return a vector of pointers to the textures required to render this volume.
     */
    virtual std::vector<ghoul::opengl::Texture*> getTextures() { return std::vector<ghoul::opengl::Texture*>(); };

    /**
     * Return a vector of integers representing the opengl buffers required to render this volume.
     */
    virtual std::vector<int> getBuffers() { return std::vector<int>(); };

    /**
     * Return the id of this volume.
     */
    int getId();

    /**
     * Get glsl name
     */
protected:
    int getTextureUnit(ghoul::opengl::Texture* texture);
    int getSsboBinding(int ssboId);

    int _id;
};          // ABufferVolume

}               // openspace

#endif  // __ABUFFER_H__
