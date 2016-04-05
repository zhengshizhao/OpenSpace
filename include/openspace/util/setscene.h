
#ifndef __SETSCENE_H__
#define __SETSCENE_H__

#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/rendering/renderengine.h> 

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
namespace openspace{

	class SetScene{


	public:
		static bool SetScene::initialize();
		static void deinitialize();
		static void update(Scene* sceneGraph);
		static std::string getNewScene();

		//static std::string currentScene;
		
	
	private:
		static float distance;
		static std::string newScene;
		//static SceneGraphNode* sceneNode;
		static int nextValue;
		//static SceneGraphNode* sceneNode;
		//int sceneNr=1;
		//static std::string startScene = "New Horizon";
	};
}
#endif