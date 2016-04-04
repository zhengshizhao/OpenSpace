
#include <openspace/util/setscene.h>

namespace {
	const std::string _loggerCat = "Set Scene";
}


namespace openspace {



	float _distance;
	std::string setScene(Scene* scene, Camera* camera, std::string _nameOfScene) {


		psc cameraPos = camera->position();
		//sets the current node to the the node with the previous name. 
		SceneGraphNode* node = scene->sceneGraphNode(_nameOfScene);

		//Starts in the last scene we kow we were in, checks if we are still inside, if not check parent, continue until we are inside a scene
		_distance = (DistanceToObject::ref().distanceCalc(camera->position(), node->worldPosition()));

		std::vector<SceneGraphNode*> childrenScene = node->children();
		int nrOfChildren = static_cast<int>(childrenScene.size());
		//traverses the scenetree to find a scene we are within. 

		while (_distance > node->sceneRadius())
		{


			_distance = (DistanceToObject::ref().distanceCalc(camera->position(), node->worldPosition()));

			//LINFO("Check parent current scene whileloop");
			if ((node->parent() != NULL) && node->parent()->name() != "SolarSystem")
			{
				node = node->parent();

				//_distance = (DistanceToObject::ref().distanceCalc(_mainCamera->position(), node->position()));
				_nameOfScene = node->name();
				childrenScene = node->children();
				nrOfChildren = static_cast<int>(childrenScene.size());


				//			LINFO(" 1: Switching to parent node named: " << _nameOfScene << " Nr of new children: " << nrOfChildren);
				break;
			}


			else if (node->parent()->name() == "SolarSystem")
			{
				//We have reached the root, solarsystem is the scene. 

				// Don't think this is needed, should already be here if we arived inside this loop.

				node = scene->allSceneGraphNodes()[3];
				_nameOfScene = node->name();
				childrenScene = node->children();
				nrOfChildren = static_cast<int>(childrenScene.size());


				//LINFO("Reached Root: " << _nameOfScene << " Nr of new children: " << nrOfChildren);


				//LINFO("Reached root node. _nameOfScene = " << _nameOfScene.c_str());
				break;
			}


		}
		//Now we know we are inside a scene. 
		//Check if we are inside a child scene of the current scene. 


		bool outsideAllChildScenes = false; // remove.



		while (!childrenScene.empty() && !outsideAllChildScenes)
		{

			for (size_t i = 0; i < nrOfChildren; ++i) //size_t
			{
				//	SceneGraphNode* tempChild = childrenScene.at(i);
				_distance = DistanceToObject::ref().distanceCalc(camera->position(), childrenScene.at(static_cast<int>(i))->worldPosition());

				if (_distance < childrenScene.at(i)->sceneRadius())
				{
					//set new current scene
					node = childrenScene.at(i);
					childrenScene = node->children();
					_nameOfScene = node->name();
					nrOfChildren = static_cast<int>(childrenScene.size());
					
					break;
				}

				if (nrOfChildren - 1 == static_cast<int>(i)){
					outsideAllChildScenes = true;
					
				}


			}


		}

		
		return _nameOfScene;

	
	}

	//remove?

	glm::mat4 collectorToMat4(glm::vec3 collector)
	{
		//glm::mat4 identityMatrix = glm::mat4(1.0f);
		glm::mat4 vm = glm::translate(glm::mat4(1.f), collector);
		return vm;
	}

	

	const glm::mat4 setNewViewMatrix(Camera* camera, Scene* scene)
	{

		//collect the positions for all parents in, get psc.vec3.
		// add them all up then use glm::translate to make it a mat 4. 
		
		psc pscViewMatrix;
		glm::vec3 collector(1.f, 1.f, 1.f);
		glm::mat4 newViewMatrix;
		glm::mat4 viewMatrix = camera->viewMatrix();;
		std::string nameOftarget = "Hydra"; //remove hardcoded target.
		std::vector<SceneGraphNode*> cameraPath;
		std::vector<SceneGraphNode*> objectPath;
		SceneGraphNode* node = scene->sceneGraphNode(camera->getParent());
		//pscViewMatrix = test.operator*(viewMatrix); 
		std::vector<SceneGraphNode*> allnodes = scene->allSceneGraphNodes();
		//std::string commonParent = findCommonParent(camera, scene);


		//Find common parent for camera and object
		std::string commonParent = camera->getParent(); // initiates to camera parent in case 
														// other path is not found

		//std::vector<SceneGraphNode*> path;
		//path.push_back(node->parent());
		//cameraPath = pathTo(node);
		cameraPath = pathTo(node);
		//std::cout << "camerapath: " << cameraPath.front() << "\n";
		objectPath = pathTo(scene->sceneGraphNode(nameOftarget)); // <--- pathTo blir null, kan inte hämta ut första elementet->crash
	
		/*
		for (int i = 0; i < objectPath.size(); i++)
		{
			std::cout << "objectPath: " << objectPath.at(i)->name() << "\n";
			std::cout << "cameraPath: " << cameraPath.at(i)->name() << "\n";
			
		}
		*/
		

		commonParent = findCommonParent(cameraPath, objectPath);//"SolarSystemBarycenter";
		
		//Find the path from the camera to the common parent
		collector = pathCollector(cameraPath, commonParent); 
		
		//SceneGraphNode* tempNode = allnodes[camera->getParent()];



		//collector;
		//glm::mat4 userMatrix = glm::translate(glm::mat4(1.f), _sgctEngine->getDefaultUserPtr()->getPos());
		//glm::mat4 sceneMatrix = _sgctEngine->getModelMatrix();
//		glm::mat4 viewMatrix = _sgctEngine->getActiveViewMatrix() * userMatrix;
//		newViewMatrix = camera->viewMatrix() * collectorToMat4(collector);
		return newViewMatrix;
	}

	glm::vec3 pathCollector(std::vector<SceneGraphNode*> path, std::string commonParent)
	{
		//commonParent
		
		SceneGraphNode* firstElement = path.front(); //uncomment  <-------------här är felet!!!
		glm::vec3 collector(path.back()->position().vec3());
/*
		for (std::vector<int>::iterator it = myvector.begin(); *it != commonParent; ++it)
			std::cout << ' ' << *it.;
		*/
		
		int depth = 0;
	
		while (firstElement->name() != commonParent)
			{
				
				//std::cout << "Node name: " << firstElement << "  Common parent: " << commonParent << "\n";
				collector = collector*firstElement->position().vec3();
				
				//std::cout << "Node Parent name: " << firstElement->parent()->name() << "\n";
				firstElement = path[depth];
				depth++;


				

			}
			
		return collector;

	}

	std::string findCommonParent(std::vector<SceneGraphNode*> t1, std::vector<SceneGraphNode*> t2) //Camera* camera, std::string nameOftarget, Scene* scene){
	{
		std::string commonParentReturn = "SolarSystemBarycenter";
		/*
		std::vector<SceneGraphNode*> t1 = pathTo(target, scene);
		std::vector<SceneGraphNode*> t2 = pathTo(nameOftarget, scene);
		*/
		


		//traverse the list backwards, position before first difference = common parent.
		 
		//traverses the lists 
		int counter = std::min(t1.size(), t2.size());
		for (int i = 0; i < counter; i++){
			if (t1.back()->name() == t2.back()->name())
				commonParentReturn = t1.back()->name();
		}
		/*
		while (t1.back() != NULL && t2.back() != NULL && t1.back() == t2.back()) // && !t1.empty && !t2.empty
		{

			commonParentReturn = t1.back()->name().c_str();
			t1.pop_back();
			t2.pop_back();
		
		}
		*/
		return commonParentReturn;
	}

	//help function for findCommonParent
	std::vector<SceneGraphNode*> pathTo(SceneGraphNode* node)//std::string nameOfScene, Scene* scene)
	{
		std::vector<SceneGraphNode*> path;
		//	path.push_back(node->parent());
		//std::vector<SceneGraphNode*> childrenScene = node->children();
	std::string name = node->name();
	
		while (name != "SolarSystemBarycenter")
		{
			path.push_back(node);
			node = node->parent();
			name = node->name();
			//node = node->parent()->parent()->name();

		}
		path.push_back(node);
		return path;
	}
}