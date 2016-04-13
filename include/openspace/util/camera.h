/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
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

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <mutex>

// glm includes
#include <ghoul/glm.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

namespace openspace {

class SyncBuffer;

class Camera {
public:
    Camera();
    ~Camera();

    void setPosition(glm::vec3 pos);
    const glm::vec3& position() const;
	
	const glm::vec3& unsynchedPosition() const;

	void setModelMatrix(glm::mat4 modelMatrix);
	const glm::mat4& modelMatrix() const;

	void setViewMatrix(glm::mat4 viewMatrix);
	const glm::mat4& viewMatrix() const;

	void setProjectionMatrix(glm::mat4 projectionMatrix);
	const glm::mat4& projectionMatrix() const;

    const glm::mat4& viewProjectionMatrix() const;

    void setCameraDirection(glm::vec3 cameraDirection);
    glm::vec3 cameraDirection() const;

	void setFocusPosition(glm::vec3 pos);
	const glm::vec3& focusPosition() const;

	void setViewRotationMatrix(glm::mat4 m);
	const glm::mat4& viewRotationMatrix() const;
    void compileViewRotationMatrix();

    void rotate(const glm::quat& rotation);
    void setRotation(glm::quat rotation);
   // const glm::quat& rotation() const;
	void setRotation(glm::mat4 rotation);

	const glm::vec3& viewDirection() const;

	const float& maxFov() const;
    const float& sinMaxFov() const;
    void setMaxFov(float fov);

    void setLookUpVector(glm::vec3 lookUp);
    const glm::vec3& lookUpVector() const;

	void postSynchronizationPreDraw();
	void preSynchronization();
	void serialize(SyncBuffer* syncBuffer);
	void deserialize(SyncBuffer* syncBuffer);

private:
    float _maxFov;
    float _sinMaxFov;
    mutable glm::mat4 _viewProjectionMatrix;
	glm::mat4 _modelMatrix;
	glm::mat4 _viewMatrix;
	glm::mat4 _projectionMatrix;
    mutable bool _dirtyViewProjectionMatrix;
    glm::vec3 _viewDirection;
    glm::vec3 _cameraDirection;
    glm::vec3 _focusPosition;
    // glm::quat _viewRotation;
    
    glm::vec3 _lookUp;

	mutable std::mutex _mutex;
	
	//local variables
	glm::mat4 _localViewRotationMatrix;
	glm::vec2 _localScaling;
	glm::vec3 _localPosition;

	//shared copies of local variables
	glm::vec2 _sharedScaling;
    glm::vec3 _sharedPosition;
	glm::mat4 _sharedViewRotationMatrix;

	//synced copies of local variables
	glm::vec2 _syncedScaling;
    glm::vec3 _syncedPosition;
	glm::mat4 _syncedViewRotationMatrix;
	
};

} // namespace openspace

#endif // __CAMERA_H__
