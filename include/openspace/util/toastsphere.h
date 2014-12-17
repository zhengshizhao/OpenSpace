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

#ifndef __TOASTSPHERE_H__
#define __TOASTSPHERE_H__

#include <openspace/rendering/planets/planetgeometry.h>
#include <openspace/properties/vectorproperty.h>
#include <openspace/properties/scalarproperty.h>
#include <ghoul/opengl/ghoul_gl.h>
#include <vector>

namespace ghoul {
	namespace opengl {
		class ProgramObject;
		class Texture;
	}
}

namespace openspace {

class RenderablePlanet;
class ToastQuadrant;

namespace planetgeometry {

class ToastSphere : public PlanetGeometry {
public:
	ToastSphere(const ghoul::Dictionary& dictionary);
	~ToastSphere();

	bool initialize(RenderablePlanet* parent) override;
	void deinitialize() override;
	void bindTexture(ghoul::opengl::ProgramObject* programObject) override;
	void loadTexture() override;
	void render() override;

	void setDetailLevel(int level);
private:
	void updateDetailLevel();
	void createOctaHedron();	

	void generateOpenGLData(int detailLevel = INT_MAX);
	void cleanupOpenGLData();

	GLsizei _numVertices;
	GLuint _VAO, _vertexPositionBuffer, _vertexToastcoordBuffer;
	properties::Vec2Property _radius;
	properties::IntProperty _maxLevel, _currentLevel;
	std::vector<ToastQuadrant*> _quadrants;
	ghoul::opengl::Texture* _texture;
};

} // namespace planetgeometry
} // namespace openspace

#endif // __TOASTSPHERE_H__