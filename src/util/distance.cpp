

#include <openspace/util/distance.h>
//#include <openspace/util/camera.h>
#include <openspace/util/powerscaledcoordinate.h>


namespace {
	const std::string _loggerCat = "Distance";
}


namespace openspace {

	//Distance::Distance(){}
	//camera position can be fetched here instead
	//only need to send target
	
	//later rewrite so nothing needs to be sent. will
	// check against list here? 

	float Distance::DistanceToObject(psc camerapos)
	{
		psc target = psc(0, 0, 0, 0);
		
		float dist = sqrt(pow((camerapos[0] - target[0]), 2) +
			pow((camerapos[1] - target[1]), 2) +
			pow((camerapos[2] - target[2]), 2));
		
		
		return dist;
	
	}



}