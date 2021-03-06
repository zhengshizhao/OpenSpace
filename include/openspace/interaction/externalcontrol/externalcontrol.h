#ifndef EXTERNALCONTROL_H
#define EXTERNALCONTROL_H

#include <openspace/util/powerscaledscalar.h>
#include <ghoul/glm.h>
#include <glm/gtc/quaternion.hpp>

namespace openspace {

class ExternalControl {
public:

    // constructors & destructor
    ExternalControl();
    virtual ~ExternalControl();
    
    virtual void update();
    
    void rotate(const glm::quat &rotation);
    void orbit(const glm::quat &rotation);
    void distance(const PowerScaledScalar &distance);

    
protected:
};

} // namespace openspace

#endif