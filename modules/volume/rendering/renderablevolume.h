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

#ifndef __RENDERABLEVOLUME_H__
#define __RENDERABLEVOLUME_H__

// open space includes
#include <openspace/rendering/renderable.h>
#include <openspace/abuffer/abuffervolume.h>
#include <openspace/properties/scalarproperty.h>

// ghoul includes
#include <ghoul/io/rawvolumereader.h>

// Forward declare to minimize dependencies
namespace ghoul {
	namespace filesystem {
		class File;
	}
	namespace opengl {
		class Texture;
	}
}

namespace openspace {

class ABufferVolume;

class RenderableVolume: public Renderable, public ABufferVolume {
public:
	// constructors & destructor
	RenderableVolume(const ghoul::Dictionary& dictionary);
	~RenderableVolume();

    virtual bool initialize() override;
    virtual bool deinitialize() override;
    virtual void preResolve(ghoul::opengl::ProgramObject* program) override;
    virtual void update(const UpdateData& data) override;
    virtual void render(const RenderData& data) override;

protected:
    ghoul::opengl::Texture* loadVolume(const std::string& filepath, const ghoul::Dictionary& hintsDictionary);
    glm::vec3 getVolumeOffset(const std::string& filepath, const ghoul::Dictionary& hintsDictionary);
    ghoul::RawVolumeReader::ReadHints readHints(const ghoul::Dictionary& dictionary);

    float _w;

    GLuint _intersectionArray;
    GLuint _intersectionVertexPositionBuffer;
    GLuint _boxArray;
    GLuint _vertexPositionBuffer;
    ghoul::opengl::ProgramObject* _boxProgram;
    ghoul::opengl::ProgramObject* _nearPlaneProgram;
    glm::vec3 _boxScaling;
    psc _pscOffset;

    glm::mat4 _modelTransform;
    std::string _modelName;
private:
    void renderIntersection(const RenderData& data);
    glm::vec3 perspectiveToCubeSpace(const RenderData& data, glm::vec4 vector);
    properties::FloatProperty _alphaCoefficient;
};

} // namespace openspace

#endif
