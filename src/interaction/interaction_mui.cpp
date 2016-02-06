#include <openspace/interaction/interaction_mui.h>

#include <openspace/engine/openspaceengine.h>

#include <openspace/interaction/interactionhandler.h>
#include <ghoul/logging/logmanager.h>

#include <Gate.h>

#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/configurationmanager.h>

#include <ghoul/filesystem/filesystem>


#define HANDLE_ERROR(val)                                                                \
    if (val != MUI_SUCCESS) {                                                            \
        throw std::runtime_error(MUI_getError());                                        \
    }

namespace openspace {
namespace interaction {

InteractionMui::InteractionMui()
    : _lastPos(0.f)
{
    std::string ipAddress = OsEng.configurationManager().value<std::string>("MorphableUITopologyServerIP");

    MUI_GateConfiguration config {{ ipAddress.c_str(), "5678" },
                                  { "localhost", "1238" },
                                  { "localhost", "1239" }};
    //MUI_GateConfiguration config{{"tcp.prefixed", "localhost", "5678"},
                                 //{"tcp.prefixed", "localhost", "1238"},
                                 //{"tcp.prefixed", "localhost", "1239"}};

    std::string applicationTypes = absPath("${CONFIG}/morphableui");
    LINFOC("InteractionMui", "Using application specification '" << applicationTypes << "'");

    try {
        HANDLE_ERROR(MUI_initialize(config));
        HANDLE_ERROR(MUI_registerGate());
        HANDLE_ERROR(
              MUI_registerApplicationTypes(applicationTypes.c_str()));
        HANDLE_ERROR(MUI_registerApplication("openspace", "OpenSpace Application"));
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
    }
}

void InteractionMui::handleInput(double deltaTime)
{
    try {
        MUI_HEVENT event;
        const uint16_t bufferSizeConst = 256;
        char requirementBuffer[bufferSizeConst];
        uint16_t bufferSize;
        while (bufferSize = bufferSizeConst, MUI_pollEvent(&event, "OpenSpace Application", requirementBuffer, &bufferSize)
               != MUI_NO_EVENT) {
            if (!strcmp(requirementBuffer, "move")) {
                float x, y, z;
                HANDLE_ERROR(MUI_eventGetFloatValue(event, "x", &x));
                HANDLE_ERROR(MUI_eventGetFloatValue(event, "y", &y));

                _handler->orbit(
                    static_cast<float>(deltaTime)  * x,
                    static_cast<float>(deltaTime)  * y,
                    static_cast<float>(deltaTime) * 0,
                    0);


                //move(x, y, 0); // what about z ???
                //LINFOC("", "Move: '" << x << "," << y);
            }
            else if (!strcmp(requirementBuffer, "rotate")) {
                float x, y, z;
                HANDLE_ERROR(MUI_eventGetFloatValue(event, "x", &x));
                HANDLE_ERROR(MUI_eventGetFloatValue(event, "y", &y));
                rotate(x, y, 0); // what about z ???
                //LINFOC("", "Rotate: '" << x << "," << y);
            }
            else if (!strcmp(requirementBuffer, "zoom")) {
                float x;
                HANDLE_ERROR(MUI_eventGetFloatValue(event, "x", &x));
                zoom(x);
                LINFOC("", "Zoom: '" << x);
            }
            MUI_destroyEvent(event);
        }
    }
    catch (const std::runtime_error& err) {
        LINFOC("mui", "ERROR: " << err.what());
    }
    catch (...) {
        LERRORC("", "Foobar");
    }
}

void InteractionMui::rotate(float x, float y, float z)
{
    glm::vec3 angles(x, y, z);
    glm::vec3 curPos = angles;

    if (_lastPos != curPos) {
        float rotationAngle = glm::angle(curPos, _lastPos);
        rotationAngle *= static_cast<float>(_handler->deltaTime() * 100.f);

        glm::vec3 rotationAxis = glm::cross(_lastPos, curPos);
        rotationAxis = glm::normalize(rotationAxis);
        glm::quat quaternion = glm::angleAxis(rotationAngle, rotationAxis);

        // Apply quaternion to camera
        _handler->orbitDelta(quaternion);

        _lastPos = curPos;
    }

    //       glm::quat rotation(angles);
    //       _handler->setRotation(rotation);

    //       _lastPos = curPos;
}

void InteractionMui::zoom(float pos)
{
    const float dt = static_cast<float>(_handler->deltaTime());
    const float speed = 2.75;

    float zoomFactor = dt * pos * speed;
    LINFOC("", zoomFactor);
    _handler->orbit(
        0.f, 0.f, 0.f,
        zoomFactor
    );
}

void InteractionMui::move(float x, float y, float z)
{
    const float dt = static_cast<float>(_handler->deltaTime());
    const float speed = 2.75;

    glm::vec3 euler(-speed * dt, 0.0, 0.0);
    glm::quat rot = glm::quat(euler);
    _handler->orbitDelta(rot);
}

glm::vec3 InteractionMui::mapToCamera(float x, float y, float z)
{
    // Get x,y,z axis vectors of current camera view
    glm::vec3 currentViewYaxis = glm::normalize(_handler->camera()->lookUpVector());
    psc viewDir = _handler->camera()->position() - _handler->focusNode()->worldPosition();
    glm::vec3 currentViewZaxis = glm::normalize(viewDir.vec3());
    glm::vec3 currentViewXaxis
          = glm::normalize(glm::cross(currentViewYaxis, currentViewZaxis));

    // mapping to camera co-ordinate
    currentViewXaxis *= x;
    currentViewYaxis *= y;
    currentViewZaxis *= z;
    return (currentViewXaxis + currentViewYaxis + currentViewZaxis);
}
}
}