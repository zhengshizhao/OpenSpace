
#include <openspace/util/setscene.h>
#include <openspace/rendering/renderengine.h> 
#include <openspace/scene/scenegraph.h>
#include <openspace/util/DistanceToObject.h>
#include <openspace/scene/scene.h>
namespace {
	const std::string _loggerCat = "Set Scene";
}


namespace openspace {
	/*

	int NextValue;
	float testing = 0;
	std::string currentScene;
	psc objpos;
	SceneGraphNode* _tmpnode = scene()->allSceneGraphNodes()[NextValue];
	currentScene = "NewHorizons";

	*/

	//sceneNr = 1;
	bool SetScene::initialize() {
		//assert(_distInstance == nullptr);
		//_distInstance = new DistanceToObject();
		//currentScene = "New Horizons";
	//	assert(sceneNode == nullptr);
		//static SceneGraphNode* sceneNode = nullptr;
		
		newScene = "";
		return true;
	}

	void SetScene::deinitialize() {
		//assert(_distInstance);
		//delete _distInstance;
		//assert(sceneNode);
	//	delete sceneNode;
		//static SceneGraphNode* sceneNode = nullptr;
	}
	
	
	std::string SetScene::getNewScene()
	{
		//update();
		return newScene;

	}
	void SetScene::update(Scene* sceneGraph){
		//SceneGraphNode* _tmpnode = scene()->allSceneGraphNodes()[1]
		//for (nextValue:nextValue++;
		//SceneGraphNode* _tmpnode = sceneGraph->allSceneGraphNodes()[nextValue];
		

		newScene ="";
		//sceneGraph->
		//SceneGraphNode* _tmpnode = sceneGraph->allSceneGraphNodes()[1]->name;
		
		//_tmpnode
		//std::string test = _tmpnode->name; //object slicing?
		
			//sceneGraph()->

		//SceneGraphNode* _tmpnode = scene()->sceneGraphNode(currentScene.c_str());

		// 1 SceneGraphNode* sceneNode = sceneGraph->allSceneGraphNodes()[1];
		//SceneGraphNode* sceneNode = sceneGraph->sceneGraphNode("New Horizons");
		//return(sceneNode);
		//sceneNode
		//sceneNode* = sceneGraph->allSceneGraphNodes()[DistanceToObject::sceneNr];
		//sceneNode->position();
		//currentScene = "NewHorizons";
		//distance = DistanceToObject::ref().distanceCalc(cameraPosition, objpos);

		//sceneGraph()->allSceneGraphNodes()[];
			//renderengine::scene()->allSceneGraphNodes()[DistanceToObject::sceneNr];
		
		//psc objpos2 = sceneGraph->position();
		//if ( distance < 50000)
		//	currentScene = DistanceToObject::node;
		
		
		//return currentScene.c_str;
	}

	
}