#include <openspace/util/setscene.h>
#include <openspace/scene/scene.h>
#include <openspace/util/camerarig.h>

namespace openspace {
	
	
			
	/*
	
	glm::vec3 CameraRig::setRelativeOrigin( Scene* scene) const{
	
	std::string target = camera->getParent();
	SceneGraphNode* commonParentNode;
	std::vector<SceneGraphNode*> commonParentPath;

	commonParentNode = scene->sceneGraphNode(target);

	
	commonParentPath = pathTo(commonParentNode);
	return newCameraOrigin(commonParentPath, target, camera, scene);


}
*/
	//ToDo remove commonParent string, its in commonParentPath[0]->name
	
	/*
	glm::vec3 CameraRig::newCameraOrigin(std::vector<SceneGraphNode*> commonParentPath, std::string commonParent, Camera* camera, Scene* scene) const {


		glm::vec3 displacementVector = camera->position().vec3();
		if (commonParentPath.size()>1){ // <1 if in root system. 

			glm::vec3 tempDvector;
			for (int i = commonParentPath.size() - 1; i >= 1; i--){
				tempDvector = commonParentPath[i - 1]->worldPosition().vec3() - commonParentPath[i]->worldPosition().vec3();
		
				displacementVector = displacementVector - tempDvector;
		
			}
		}

		//Move the camera to the position of the common parent (The new origin)
		//Then add the distance from the displacementVector to get the camera into correct position
		glm::vec3 origin = commonParentPath[0]->position().vec3();

		glm::vec3 newOrigin;
		camera->setDisplacementVector(displacementVector);
		newOrigin[0] = (origin[0] + displacementVector[0]);
		newOrigin[1] = (origin[1] + displacementVector[1]);
		newOrigin[2] = (origin[2] + displacementVector[2]);

		newOrigin = origin + displacementVector;
		//camera->setPosition(newOrigin);
		return(newOrigin);
	}
	
	
	*/
	
	
	
	
	
	
	
	
	
	/*
	static const glm::mat4 setNewViewMatrix(std::string cameraParent, SceneGraphNode* target, Scene* scene) const
	{

		//collect the positions for all parents in, get psc.vec3.
		// add them all up then use glm::translate to make it a mat 4. 

		//sceneGraphNode(camera->getParent())
		//Scene scene = Scene::Scene();
		psc pscViewMatrix;
		glm::vec3 collectorCam(1.f, 1.f, 1.f);
		glm::vec3 collectorObj(1.f, 1.f, 1.f);

		glm::mat4 newViewMatrix;
		//glm::mat4 viewMatrix = camera->viewMatrix();;
		std::string nameOftarget = target->name();
		std::vector<SceneGraphNode*> cameraPath;
		std::vector<SceneGraphNode*> objectPath;
		SceneGraphNode* node = scene->sceneGraphNode(cameraParent);//Scene->sceneGraphNode(camera->getParent());
		SceneGraphNode* commonParentNode;
		std::vector<SceneGraphNode*> commonParentPath;
		//	std::vector<SceneGraphNode*> allnodes = scene->allSceneGraphNodes();

		//Scene scene = RenderEngine::scene()

		//Find common parent for camera and object
		std::string commonParentName = cameraParent; // initiates to camera parent in case 
		// other path is not found


		cameraPath = pathTo(node);

		objectPath = pathTo(scene->sceneGraphNode(nameOftarget));


		commonParentNode = findCommonParentNode(cameraParent, nameOftarget, scene);
		commonParentName = commonParentNode->name();
		commonParentPath = pathTo(commonParentNode);

		//Find the path from the camera to the common parent
		collectorCam = pathCollector(cameraPath, commonParentName, false);
		collectorObj = pathCollector(objectPath, commonParentName, false);




		glm::vec3 translationCollector;

		for (int j = 0; j <= 2; j++){

			translationCollector[j] = collectorObj[j] + collectorCam[j];


		}
		newViewMatrix = glm::translate(newViewMatrix, translationCollector);




		return glm::transpose(newViewMatrix);
	}

	*/
}