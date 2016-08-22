

#ifndef __SETSCENE_H__
#define __SETSCENE_H__

#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/rendering/renderengine.h> 
#include <openspace/util/distancetoobject.h>
#include <openspace/scene/scenegraphnode.h>
#include <glm/gtc/type_ptr.hpp>
#include <openspace/scene/scene.h>

#include <iostream>

namespace openspace {



    std::string setScene(Scene* scene, Camera* camera, std::string _nameOfScene);

    const glm::mat4 setNewViewMatrix(std::string, SceneGraphNode* target, Scene*);

    glm::vec3 pathCollector(std::vector<SceneGraphNode*> path, std::string commonParent, bool inverse);

    std::vector<SceneGraphNode*> pathTo(std::string nameOfScene, Scene* scene);
    void setRelativeOrigin(Camera* camera, Scene* scene);

    void newCameraOrigin(std::vector<SceneGraphNode*> commonParentPath, std::string commonParent, Camera* camera, Scene* scene);

    glm::vec3 Vec3Subtract(glm::vec3 vec1, glm::vec3 vec2);

    std::string commonParent(std::vector<SceneGraphNode*> t1, std::vector<SceneGraphNode*> t2);

    SceneGraphNode* findCommonParentNode(std::string firstPath, std::string secondPath, Scene* scene);

    std::vector<SceneGraphNode*> pathTo(SceneGraphNode* node);
    
    //std::string nameOfScene, Scene* scene)

    //std::string findCommonParent(Camera* camera, Scene* scene);
}
#endif