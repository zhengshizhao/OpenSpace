
#include <openspace/util/DistanceToObject.h>
#include <cassert>
//#include <openspace/util/camera.h>
#include <openspace/util/powerscaledcoordinate.h>
//#include <assert.h>

#include <openspace/engine/openspaceengine.h>
#include <openspace/interaction/interactionhandler.h>
#include <openspace/util/constants.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/syncbuffer.h>

#include <ghoul/filesystem/filesystem.h>

namespace {
	const std::string _loggerCat = "Distance to object";
}


namespace openspace {



	//DistanceToObject* DistanceToObject::_distancefromcamera = nullptr;
	//Distance::Distance(){}
	//camera position can be fetched here instead
	//only need to send target

	//later rewrite so nothing needs to be sent. will
	// check against list here? 

	/*
	float DistanceToObject::distanceCalc() //psc& camerapos
	{
		
		psc target = psc(0, 0, 0, 0);

		float dist = sqrt(pow((camerapos[0] - target[0]), 2) +
			pow((camerapos[1] - target[1]), 2) +
			pow((camerapos[2] - target[2]), 2));
		
		float dist = 10001.0;

		return dist;

	}*/

	DistanceToObject* DistanceToObject::_distInstance = nullptr;
	DistanceToObject::DistanceToObject() {}
	int sceneNr = 1;
	
bool DistanceToObject::initialize() {
	assert(_distInstance == nullptr);
	_distInstance = new DistanceToObject();
	assert(sceneNr == 1);
	//sceneNr = 1;
	
	
		return true;
	}

void DistanceToObject::deinitialize() {
		assert(_distInstance);
		delete _distInstance;
		_distInstance = nullptr;
	}
	
	

DistanceToObject& DistanceToObject::ref() {
	assert(_distInstance);
	return *_distInstance;
	}

	
/*
static bool isInitialized(){

	return (_instance != nullptr);
	
	
	}*/



float DistanceToObject::distanceCalc(const psc& positionPSC, psc targetPosPSC) //psc& camerapos
	{
		//assert(_distInstance);
		//sceneNr++;
		//objpos = _tmpnode->position();
		// ***fix to work with psc*** (remove PSC from input when PSC gone)
		glm::dvec3 targetPos = targetPosPSC.dvec3();
		glm::dvec3 position = positionPSC.dvec3();
		
		distance = sqrt(pow(position[0] - targetPos[0], 2) + pow(position[1] - targetPos[1], 2) + pow(position[2] - targetPos[2], 2));
		
		
		//*_distInstance = &distance;
		//if (distance < distToScene) {}
		//sceneNrPub = sceneNr;
		return distance;

		//_instance = std::move(value);
		//return distance;
	}
	

	
}