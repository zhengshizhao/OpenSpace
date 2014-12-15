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
	std::vector<glm::vec4> vertices;
	std::vector<glm::vec2> toastCoords;
	Quadrant(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3) {
		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p0);
		vertices.push_back(p3);
		vertices.push_back(p1);
	}
	Quadrant(glm::vec4 p0,  glm::vec4 p1,  glm::vec4 p2,  glm::vec4 p3,
			 glm::vec2 tc0, glm::vec2 tc1, glm::vec2 tc2, glm::vec2 tc3) {
		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p0);
		vertices.push_back(p3);
		vertices.push_back(p1);

		toastCoords.push_back(tc0);
		toastCoords.push_back(tc1);
		toastCoords.push_back(tc2);
		toastCoords.push_back(tc0);
		toastCoords.push_back(tc3);
		toastCoords.push_back(tc1);
	}
};

class ToastSphere {
public:
<<<<<<< HEAD
	ToastSphere(PowerScaledScalar radius, int levels);
=======
	ToastSphere(PowerScaledScalar radius);
>>>>>>> 87fae3f5bcbab03c18af20d790989b61095a71b5
	bool initialize();
	void render();
private:
	void createOctaHedron();
	std::vector<Quadrant> subdivide(Quadrant q, int levels);

	GLuint _VAO;
<<<<<<< HEAD
	pss _radius;
	int _levels;
=======
	pss _radius;	
>>>>>>> 87fae3f5bcbab03c18af20d790989b61095a71b5
	std::vector<Quadrant> _quadrants;	
};

} // namespace openspace

#endif // __TOASTSPHERE_H__