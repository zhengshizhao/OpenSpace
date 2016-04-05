/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2015                                                               *
*                                                                                       *
* Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
* software and associated documentation files (the "Software"), to deal in the Software *
* without restriction, including without limitation the rights to use, copy, modify,    *
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
* permit persons to whom the Software is furnished to do so, subject to the following   *
* conditions:                                                                           *
*                                                                                       *
* The above copyright notice and this permission notice shall be included in all copies *
* or substantial portions of the Software.                                              *
*                                                                                       *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
****************************************************************************************/


#include <openspace/util/factorymanager.h>
#include <modules/newhorizons/util/setscene.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/time.h>
//updates to the current scene. Checks every frame. 


namespace {
	const std::string _loggerCat = "Set Current Scene";
}



namespace openspace {
	
	
	void printTest()
	{
		LINFO("Setscene");
	}

	float distToSurf(std::string object1)
	{
		float a, b, c;
		glm::vec3 nhPos;
		double currentTime = Time::ref().currentTime();
		double lt;
		SpiceManager::ref().getTargetPosition("PLUTO", "NEW HORIZONS", "GALACTIC", "NONE", currentTime, nhPos, lt);

		SpiceManager::ref().getPlanetEllipsoid("PLUTO", a, b, c);
		float radius = (a + b) / 2.f;
		float distToSurf = glm::length(nhPos) - radius;
		//with psc
		// float distToSurf = glm::length(nhPos.vec3()) - radius;
		return distToSurf;
	}
	//SetScene::UpdateScene(){}
	//check if we are withing the scene we were before.
	//check distance to parent from camera pos.
	//compare this with the radius variable of the parent.
	//return true or false.


	//while above true
	//check if inside children scene, if yes, set child scene to parent.
	
	//changes the current scene.




	//CalculateDistance()	{		}

	//ProximityCheck() {} // check if we are within a certain scene, send distance from CD above. 




	


}





/*


glm::vec4 position;
if (cameraDictionary.hasKey(KeyPositionObject)
&& cameraDictionary.getValue(KeyPositionObject, position)) {

LDEBUG("Camera position is ("
<< position[0] << ", "
<< position[1] << ", "
<< position[2] << ", "
<< position[3] << ")");

cameraPosition = psc(position);
//c->setPosition(position);
}
*/

/*
// part of scalegraph// -nh

list of objects




*/

/*
first check if still in same scene.

// issues if we enter a scene of a different parent ^^
Solution:
1 Check everything --V
2 Start check at parents parent.
3 Obj that travel through other scenes, sattelites/astroids etc
are in a seperate checklist as well. (double list solution)

// objects that travel through other systems is also in a


while scene = not found{

if distanceToSceneOrigin(currentscene.origin,camerapos) < currentscene.Radius
// we have left our current scene.
// change current scene to parent of the current scene.

currentscene = currentscene.parent.  //does this work? what will
//happen first.
//Issues may occur if we leave a scene and its parent at the
//same time, this should not happen if scenes are created
//correctly with sufficient buffer.

//				(subscene)		(parentscene)
//		(_ _ _(<--r--0--r-->)_ _ _ _ 0 _ _ _ _ _ _ _ _ _ _ _ _ _ )
//		   ^-buffer radius in parent scene

// After this we should have the correct parent,
// all we have to do now is to check if we have entered any child scene

// loop through list of children,
// double list solution
// create list


// (maybe dont need to create new list, append parent.listofchildren to
// ListOfTraversalObj, (save nr of obj in traversal first), remove
// Remove from list at the end. whats quicker? )

itemlist = parent.listofChuildren
itemlist.append(ListOfTraversalObj)


current_item = itemlist[0]
//whiledo?

while current_item { //current item = Null when done or similar


//the scale will automatically be correct
//we compare the distance to the radius that is already in
//the correct scale

if distanceToSceneOrigin(item.origin,camerapos) < current_item.Radius{
currentscene = current_item
itemlist = parent.listofChuildren	//creating a new list to check.
//so all children of the scene we enter will be checked,
//no matter how far down the "tree"

itemlist.append(ListOfTraversalObj)
i=-1; //-1 to account for the counter, will end up being the first element.
}

current_item = itemlist[i++]
}

*/


/*
distanceToSceneOrigin::(mat4 origin, mat4 camerapos){
vectToDestination = origin - camerapos
return	sqr(vectToDestination*vectToDestination)
}

*/