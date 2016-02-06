// #include <openspace/interaction/interactionhandler.h>

#ifndef __INTERACTION_MUI_H__
#define __INTERACTION_MUI_H__

#include <iostream>
#include <memory>

#include <openspace/interaction/controller.h>

// #include <openspace/interaction/mouse.h>

#include <ghoul/glm.h>

namespace openspace {
namespace interaction {

class InteractionMui : public Controller {
public:
	InteractionMui();
    virtual ~InteractionMui() {}
    
    void handleInput(double deltaTime);
    void move(float x, float y, float z);
    void rotate(float x, float y, float z);
    void zoom(float pos);

protected:
	glm::vec3 _lastPos;
// 	glm::vec3 _lastTrackballPos;
// 	glm::mat4 la;
// 	bool _isMouseBeingPressedAndHeld;
// 
// 	glm::vec3 mapToTrackball(glm::vec2 mousePos);
// 
	glm::vec3 mapToCamera(float x, float y, float z);
// 
// 	void trackballRotate(int x, int y);
};
}
}

#endif