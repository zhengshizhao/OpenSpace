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

#include <ghoul/opengl/ghoul_gl.h>
#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/util/powerscaledscalar.h>
#include <vector>

namespace openspace {

struct Quadrant {
	glm::vec4 v0, v1, v2, v3, v4, v5;
	Quadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3) {
		v0 = p0;
		v1 = p1;
		v2 = p2;
		v3 = p0;
		v4 = p3;
		v5 = p1;
	}
};

class ToastSphere {
public:
	ToastSphere(PowerScaledScalar radius);
	bool initialize();
	void render();
private:
	void createOctaHedron();
	std::vector<Quadrant> subdivide(Quadrant q, int levels);

	GLuint _VAO;
	pss _radius;	
	std::vector<Quadrant> _quadrants;	
};

} // namespace openspace

#endif // __TOASTSPHERE_H__