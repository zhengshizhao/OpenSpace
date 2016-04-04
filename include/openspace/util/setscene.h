
#ifndef __SETSCENE_H__
#define __SETSCENE_H__

#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/rendering/renderengine.h> 
#include <openspace/util/DistanceToObject.h>
#include <openspace/scene/scenegraphnode.h>
#include <glm/gtc/type_ptr.hpp>
#include <openspace/scene/scene.h>

#include <iostream>
namespace openspace{



	std::string setScene(Scene* scene, Camera* camera, std::string _nameOfScene);

	const glm::mat4 setNewViewMatrix(Camera* camera, Scene* scene);

	glm::vec3 pathCollector(std::vector<SceneGraphNode*> path, std::string commonParent);

	std::vector<SceneGraphNode*> pathTo(std::string nameOfScene, Scene* scene);
	
	
	std::string findCommonParent(std::vector<SceneGraphNode*> t1, std::vector<SceneGraphNode*> t2);
	
	std::vector<SceneGraphNode*> pathTo(SceneGraphNode* node);//std::string nameOfScene, Scene* scene)
	



	//
	//std::string findCommonParent(Camera* camera, Scene* scene);
}
#endif