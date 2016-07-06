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

#include <openspace/rendering/renderengine.h> 

#ifdef OPENSPACE_MODULE_NEWHORIZONS_ENABLED
#include <modules/newhorizons/util/imagesequencer.h>
#endif

#include <openspace/abuffer/abuffervisualizer.h>
#include <openspace/abuffer/abuffer.h>
#include <openspace/abuffer/abufferframebuffer.h>
#include <openspace/abuffer/abuffersinglelinked.h>
#include <openspace/abuffer/abufferfixed.h>
#include <openspace/abuffer/abufferdynamic.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/interaction/interactionhandler.h>
#include <openspace/scene/scene.h>
#include <openspace/util/camera.h>
#include <openspace/util/DistanceToObject.h>
#include <openspace/util/setscene.h>
#include <openspace/util/constants.h>
#include <openspace/util/time.h>
#include <openspace/util/screenlog.h>
#include <openspace/util/spicemanager.h>
//#include <openspace/rendering/renderablepath.h>
#include <modules/base/rendering/renderablepath.h>
#include <openspace/util/syncbuffer.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/sharedmemory.h>
#include <openspace/engine/configurationmanager.h>
#include <ghoul/systemcapabilities/systemcapabilities.h>
#include <ghoul/systemcapabilities/openglcapabilitiescomponent.h>
#include <openspace/util/setOverrideViewMatrix.h>//test

#include <ghoul/io/texture/texturereader.h>
#ifdef GHOUL_USE_DEVIL
#include <ghoul/io/texture/texturereaderdevil.h>
#endif //GHOUL_USE_DEVIL
#ifdef GHOUL_USE_FREEIMAGE
#include <ghoul/io/texture/texturereaderfreeimage.h>
#endif // GHOUL_USE_FREEIMAGE
#include <ghoul/io/texture/texturereadercmap.h>

#include <array>
#include <fstream>
#include <sgct.h>

// These are temporary ---abock
#include <modules/base/ephemeris/spiceephemeris.h>
#include <modules/base/ephemeris/staticephemeris.h>

// ABuffer defines
#define ABUFFER_FRAMEBUFFER 0
#define ABUFFER_SINGLE_LINKED 1
#define ABUFFER_FIXED 2
#define ABUFFER_DYNAMIC 3

#include "renderengine_lua.inl"

namespace {
	const std::string _loggerCat = "RenderEngine";

    const std::string KeyRenderingMethod = "RenderingMethod";
    const std::string DefaultRenderingMethod = "ABufferSingleLinked";
}

namespace openspace {

const std::string RenderEngine::PerformanceMeasurementSharedData =
	"OpenSpacePerformanceMeasurementSharedData";

RenderEngine::RenderEngine()
	: _mainCamera(nullptr)
	, _sceneGraph(nullptr)
	, _abuffer(nullptr)
    , _abufferImplementation(ABufferImplementation::Invalid)
	, _log(nullptr)
	, _showInfo(true)
	, _showScreenLog(true)
	, _takeScreenshot(false)
	, _doPerformanceMeasurements(false)
	, _performanceMemory(nullptr)
	, _globalBlackOutFactor(1.f)
	, _fadeDuration(2.f)
	, _currentFadeTime(0.f)
	, _fadeDirection(0)
    , _sgctRenderStatisticsVisible(false)
    , _visualizeABuffer(false)
    , _visualizer(nullptr)
	, _sceneNumber(0)
	, _distance(0.f)
	,_nameOfScene("SolarSystemBarycenter")
{
    _onScreenInformation = {
        glm::vec2(0.f),
        12,
        -1
    };
}

RenderEngine::~RenderEngine() {
	delete _abuffer;
	_abuffer = nullptr;

	delete _sceneGraph;
	_sceneGraph = nullptr;

	delete _mainCamera;
	delete _visualizer;

	delete _performanceMemory;
	if (ghoul::SharedMemory::exists(PerformanceMeasurementSharedData))
		ghoul::SharedMemory::remove(PerformanceMeasurementSharedData);
}

bool RenderEngine::initialize() {
    std::string renderingMethod = DefaultRenderingMethod;
    
    // If the user specified a rendering method that he would like to use, use that
    if (OsEng.configurationManager()->hasKeyAndValue<std::string>(KeyRenderingMethod))
        renderingMethod = OsEng.configurationManager()->value<std::string>(KeyRenderingMethod);
    else {
        using Version = ghoul::systemcapabilities::OpenGLCapabilitiesComponent::Version;

        // The default rendering method has a requirement of OpenGL 4.3, so if we are
        // below that, we will fall back to frame buffer operation
        if (OpenGLCap.openGLVersion() < Version(4,3)) {
            LINFO("Falling back to framebuffer implementation due to OpenGL limitations");
            renderingMethod = "ABufferFrameBuffer";
        }
    }

    _abufferImplementation = aBufferFromString(renderingMethod);
    switch (_abufferImplementation) {
    case ABufferImplementation::FrameBuffer:
        LINFO("Creating ABufferFramebuffer implementation");
        _abuffer = new ABufferFramebuffer;
        break;
    case ABufferImplementation::SingleLinked:
        LINFO("Creating ABufferSingleLinked implementation");
        _abuffer = new ABufferSingleLinked();
        break;
    case ABufferImplementation::Fixed:
        LINFO("Creating ABufferFixed implementation");
        _abuffer = new ABufferFixed();
        break;
    case ABufferImplementation::Dynamic:
        LINFO("Creating ABufferDynamic implementation");
        _abuffer = new ABufferDynamic();
        break;
    case ABufferImplementation::Invalid:
        LFATAL("Rendering method '" << renderingMethod << "' not among the available "
            << "rendering methods");
        return false;
    }

	generateGlslConfig();

	// init camera and set temporary position and scaling
	_mainCamera = new Camera();
	_mainCamera->setScaling(glm::vec2(1.0, -8.0));
	_mainCamera->setPosition(psc(0.f, 0.f, 1.499823f, 11.f));
	
	OsEng.interactionHandler()->setCamera(_mainCamera);

#ifdef GHOUL_USE_DEVIL
	ghoul::io::TextureReader::ref().addReader(new ghoul::io::impl::TextureReaderDevIL);
#endif // GHOUL_USE_DEVIL
#ifdef GHOUL_USE_FREEIMAGE
    ghoul::io::TextureReader::ref().addReader(new ghoul::io::impl::TextureReaderFreeImage);
#endif // GHOUL_USE_FREEIMAGE

	ghoul::io::TextureReader::ref().addReader(new ghoul::io::impl::TextureReaderCMAP);


	return true;
}

bool RenderEngine::initializeGL() {
	// LDEBUG("RenderEngine::initializeGL()");
	sgct::SGCTWindow* wPtr = sgct::Engine::instance()->getActiveWindowPtr();

	// TODO:    Fix the power scaled coordinates in such a way that these 
	//			values can be set to more realistic values

	// set the close clip plane and the far clip plane to extreme values while in
	// development
	sgct::Engine::instance()->setNearAndFarClippingPlanes(0.001f, 1000.0f);
	// sgct::Engine::instance()->setNearAndFarClippingPlanes(0.1f, 30.0f);

	// calculating the maximum field of view for the camera, used to
	// determine visibility of objects in the scene graph
	if (wPtr->isUsingFisheyeRendering()) {
		// fisheye mode, looking upwards to the "dome"
		glm::vec4 upDirection(0, 1, 0, 0);

		// get the tilt and rotate the view
		const float tilt = wPtr->getFisheyeTilt();
		glm::mat4 tiltMatrix
			= glm::rotate(glm::mat4(1.0f), tilt, glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::vec4 viewdir = tiltMatrix * upDirection;

		// set the tilted view and the FOV
		_mainCamera->setCameraDirection(glm::vec3(viewdir[0], viewdir[1], viewdir[2]));
		_mainCamera->setMaxFov(wPtr->getFisheyeFOV());
		_mainCamera->setLookUpVector(glm::vec3(0.0, 1.0, 0.0));
	}
	else {
		// get corner positions, calculating the forth to easily calculate center
		glm::vec3 corners[4];
		corners[0] = wPtr->getCurrentViewport()->getViewPlaneCoords(
			sgct_core::Viewport::LowerLeft);
		corners[1] = wPtr->getCurrentViewport()->getViewPlaneCoords(
			sgct_core::Viewport::UpperLeft);
		corners[2] = wPtr->getCurrentViewport()->getViewPlaneCoords(
			sgct_core::Viewport::UpperRight);
		corners[3] = glm::vec3(corners[2][0], corners[0][1], corners[2][2]);
		const glm::vec3 center = (corners[0] + corners[1] + corners[2] + corners[3])
			/ 4.0f;

			
//#if 0
//			// @TODO Remove the ifdef when the next SGCT version is released that requests the
//			// getUserPtr to get a name parameter ---abock
//
//			// set the eye position, useful during rendering
//			const glm::vec3 eyePosition
//				= sgct_core::ClusterManager::instance()->getUserPtr("")->getPos();
//#else
//			const glm::vec3 eyePosition
//				= sgct_core::ClusterManager::instance()->getUserPtr()->getPos();
//#endif
		const glm::vec3 eyePosition = sgct_core::ClusterManager::instance()->getDefaultUserPtr()->getPos();
		// get viewdirection, stores the direction in the camera, used for culling
		const glm::vec3 viewdir = glm::normalize(eyePosition - center);
		_mainCamera->setCameraDirection(-viewdir);
		_mainCamera->setLookUpVector(glm::vec3(0.0, 1.0, 0.0));

		// set the initial fov to be 0.0 which means everything will be culled
		float maxFov = 0.0f;

		// for each corner
		for (int i = 0; i < 4; ++i) {
			// calculate radians to corner
			glm::vec3 dir = glm::normalize(eyePosition - corners[i]);
			float radsbetween = acos(glm::dot(viewdir, dir))
				/ (glm::length(viewdir) * glm::length(dir));

			// the angle to a corner is larger than the current maxima
			if (radsbetween > maxFov) {
				maxFov = radsbetween;
			}
		}
		_mainCamera->setMaxFov(maxFov);
	}

    LINFO("Initializing ABuffer");
	_abuffer->initialize();

    LINFO("Initializing Log");
	_log = new ScreenLog();
	ghoul::logging::LogManager::ref().addLog(_log);

    LINFO("Initializing Visualizer");
	_visualizer = new ABufferVisualizer();

    LINFO("Finished initializing GL");
	return true;
}

void RenderEngine::preSynchronization() {
	if (_mainCamera)
		_mainCamera->preSynchronization();
}

void RenderEngine::postSynchronizationPreDraw() {
    sgct::Engine::instance()->setStatsGraphVisibility(_sgctRenderStatisticsVisible);
	//temporary fade funtionality
	if (_fadeDirection != 0) {
		if (_currentFadeTime > _fadeDuration){
			_fadeDirection = 0;
			_globalBlackOutFactor = fminf(1.f, fmaxf(0.f, _globalBlackOutFactor));
		} 
		else {
			if (_fadeDirection < 0)
				_globalBlackOutFactor = glm::smoothstep(1.f, 0.f, _currentFadeTime / _fadeDuration);
			else
				_globalBlackOutFactor = glm::smoothstep(0.f, 1.f, _currentFadeTime / _fadeDuration);
			_currentFadeTime += static_cast<float>(sgct::Engine::instance()->getAvgDt());
		}
	}

	if (_mainCamera)
		_mainCamera->postSynchronizationPreDraw();

	sgct_core::SGCTNode* thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
	
	
	bool updateAbuffer = false;
	for (unsigned int i = 0; i < thisNode->getNumberOfWindows(); i++) {
		if (sgct::Engine::instance()->getWindowPtr(i)->isWindowResized()) {
			updateAbuffer = true;
			break;
		}
	}
	if (updateAbuffer) {
		generateGlslConfig();
		_abuffer->reinitialize();
	}

	// converts the quaternion used to rotation matrices
	
	
	// update and evaluate the scene starting from the root node
	
	_sceneGraph->update({
		Time::ref().currentTime(),
        Time::ref().timeJumped(),
		Time::ref().deltaTime(),
		_doPerformanceMeasurements
	});
	//moved camera after _sceneGraph update since scenegraph needs to be updated first in order to find paths.
	//Sets the camera to its relative position depending on the common parent (when changed from worldPosition to position)
	if (_mainCamera){
		setRelativeOrigin(_mainCamera, scene());
		_mainCamera->compileViewRotationMatrix();
		//setRelativeOrigin(_mainCamera, scene());
		//_mainCamera->setPosition(origin);
		//setNewViewMatrix
	}
	
	//Hitta vart jag ska sätta dessa och om det var view eller view projection.
	//glm::mat4 wm = setNewViewMatrix(_mainCamera, scene(), scene()->sceneGraphNode("PlutoProjection"));	
	//_mainCamera->setViewProjectionMatrix(wm);

	
	_sceneGraph->evaluate(_mainCamera);
	
	// clear the abuffer before rendering the scene
	_abuffer->clear();

	//Allow focus node to update camera (enables camera-following)
	//FIX LATER: THIS CAUSES MASTER NODE TO BE ONE FRAME AHEAD OF SLAVES
	//if (const SceneGraphNode* node = OsEng.ref().interactionHandler().focusNode()){
		//node->updateCamera(_mainCamera);
	//}

}

void RenderEngine::render(const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) {
	// We need the window pointer
	sgct::SGCTWindow* w = sgct::Engine::instance()->getActiveWindowPtr();
	if (w->isUsingFisheyeRendering())
		_abuffer->clear();


	// SGCT resets certain settings
    
    if (_abufferImplementation == ABufferImplementation::FrameBuffer) {
        glEnable(GL_DEPTH_TEST);
        //			glDisable(GL_CULL_FACE);
        glEnable(GL_CULL_FACE);
        //			glDisable(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }
	// setup the camera for the current frame
	
	_mainCamera->setViewMatrix(viewMatrix);
		//setNewViewMatrix(_mainCamera, scene()));
	_mainCamera->setProjectionMatrix(
		projectionMatrix);
	//Is this really necessary to store? @JK
	_mainCamera->setViewProjectionMatrix(projectionMatrix * viewMatrix);

    // We only want to skip the rendering if we are the master and we want to
    // disable the rendering for the master
    if (!(OsEng.isMaster() && _disableMasterRendering)) {
        if (!_visualizeABuffer) {
            _abuffer->preRender();
            _sceneGraph->render({
                *_mainCamera,
                psc(),
                _doPerformanceMeasurements
            });
            _abuffer->postRender();

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            _abuffer->resolve(_globalBlackOutFactor);
            glDisable(GL_BLEND);
        }
        else {
            _visualizer->render();
        }
    }

#if 1
#define PrintText(__i__, __format__, ...) Freetype::print(font, 10.f, static_cast<float>(startY - font_size_mono * __i__ * 2), __format__, __VA_ARGS__);
#define PrintColorTextArg(__i__, __format__, __size__, __color__, ...) Freetype::print(font, __size__, static_cast<float>(startY - font_size_mono * __i__ * 2), __color__, __format__, __VA_ARGS__);
#define PrintColorText(__i__, __format__, __size__, __color__) Freetype::print(font, __size__, static_cast<float>(startY - font_size_mono * __i__ * 2), __color__, __format__);
	
    if (_onScreenInformation._node != -1) {
        //int thisId = sgct_core::ClusterManager::instance()->getThisNodeId();

        //if (thisId == _onScreenInformation._node) {
            //const unsigned int font_size_mono = _onScreenInformation._size;
            //int x1, xSize, y1, ySize;
            //const sgct_text::Font* font = sgct_text::FontManager::instance()->getFont(constants::fonts::keyMono, font_size_mono);
            //sgct::Engine::instance()->getActiveWindowPtr()->getCurrentViewportPixelCoords(x1, y1, xSize, ySize);
            //int startY = ySize - 2 * font_size_mono;
        //}
    }

	// Print some useful information on the master viewport
	if (OsEng.ref().isMaster() && !w->isUsingFisheyeRendering()) {

		// TODO: Adjust font_siz114e properly when using retina screen
		const int font_size_mono = 10;
        const int font_size_time = 15;
		const int font_size_light = 8;
		const int font_with_light = static_cast<int>(font_size_light*0.7);
		const sgct_text::Font* fontLight = sgct_text::FontManager::instance()->getFont(constants::fonts::keyLight, font_size_light);
		const sgct_text::Font* fontMono = sgct_text::FontManager::instance()->getFont(constants::fonts::keyMono, font_size_mono);
        const sgct_text::Font* fontTime = sgct_text::FontManager::instance()->getFont(constants::fonts::keyMono, font_size_time);

		if (_showInfo) {
			const sgct_text::Font* font = fontMono;
			int x1, xSize, y1, ySize;
			sgct::Engine::instance()->getActiveWindowPtr()->getCurrentViewportPixelCoords(x1, y1, xSize, ySize);
			int startY = ySize - 2 * font_size_time;
			//const glm::vec2& scaling = _mainCamera->scaling();
			//const glm::vec3& viewdirection = _mainCamera->viewDirection();
			//const psc& position = _mainCamera->position();
			//const psc& origin = OsEng.interactionHandler()->focusNode()->worldPosition();
			//const PowerScaledScalar& pssl = (position - origin).length();
					
			// Next 2 lines neccesary for instrument switching to work. 
			// GUI PRINT 
			// Using a macro to shorten line length and increase readability

            int line = 0;
			
			double currentTime = Time::ref().currentTime();

			//PrintText(line++, "Date: %s", Time::ref().currentTimeUTC().c_str());
            std::string timeString = Time::ref().currentTimeUTC();
            if (timeString.size() > 11)
                // This should never happen, but it's an emergency hack ---abock
                timeString[11] = ' ';
            Freetype::print(fontTime, 10, static_cast<float>(startY - font_size_mono * line++ * 2), glm::vec4(1), "Date: %s", timeString.c_str());
			
            glm::vec4 targetColor(0.00, 0.75, 1.00, 1);
			double dt = Time::ref().deltaTime();
			PrintColorTextArg(line++, "Simulation increment (s): %.0f", 10, glm::vec4(1), dt);

			psc nhPos;
			double lt;
			SpiceManager::ref().getTargetPosition("PLUTO", "NEW HORIZONS", "GALACTIC", "NONE", currentTime, nhPos, lt);
			//nhPos[3] += 3;
			float a, b, c;
			SpiceManager::ref().getPlanetEllipsoid("PLUTO", a, b, c);
			float radius = (a + b) / 2.f;

			float distToSurf = glm::length(nhPos.vec3()) - radius;
			PrintText(line++, "Distance to Pluto: % .1f (KM)", distToSurf);

			PrintText(line++, "Avg. Frametime: %.5f", sgct::Engine::instance()->getAvgDt());
			

		
			
			_mainCamera->setParent(_nameOfScene);
			psc campos = _mainCamera->position();
			psc parentLocalPos = scene()->sceneGraphNode(_mainCamera->getParent())->position();
			psc parentWorldPos = scene()->sceneGraphNode(_mainCamera->getParent())->worldPosition();
			glm::vec3 dispV = _mainCamera->getDisplacementVector();
			
			psc earthPos = scene()->sceneGraphNode("Earth")->position();
			psc earthWPos = scene()->sceneGraphNode("Earth")->worldPosition();
			/*
			psc camUnsynched = _mainCamera->unsynchedPosition();
			psc JupiterPos = scene()->sceneGraphNode("JupiterBarycenter")->worldPosition();

			
			PrintText(line++, "position for camera =         (% .5f, % .5f, % .5f% .5f)", campos[0], campos[1], campos[2], campos[3]);
			/*
			PrintText(line++, "position for camUnsynched =         (% .5f, % .5f, % .5f% .5f)", camUnsynched[0], camUnsynched[1], camUnsynched[2], camUnsynched[3]);
			PrintText(line++, "World position for JupiterPos =         (% .5f, % .5f, % .5f% .5f)", JupiterPos[0], JupiterPos[1], JupiterPos[2], JupiterPos[3]);
			*/
			PrintText(line++, "Cammera parent: (%s)", _mainCamera->getParent().c_str());
			PrintText(line++, "position for camera =               (% .5f, % .5f, % .5f% .5f)", campos[0], campos[1], campos[2], campos[3]);
			PrintText(line++, "Local Position for parent =         (% .5f, % .5f, % .5f% .5f)", parentLocalPos[0], parentLocalPos[1], parentLocalPos[2], parentLocalPos[3]);
			PrintText(line++, "Displacement Vector  =              (% .5f, % .5f, % .5f% .5f)", dispV[0], dispV[1],  dispV[2], 0.0f);
			PrintText(line++, "Camera parent + displacementnode    (% .5f, % .5f, % .5f% .5f)", parentLocalPos[0] + dispV[0], parentLocalPos[1] + dispV[1], parentLocalPos[2] + dispV[2], parentLocalPos[3]);
			PrintText(line++, "World position for parent =         (% .5f, % .5f, % .5f% .5f)", parentWorldPos[0], parentWorldPos[1], parentWorldPos[2], parentWorldPos[3]);
			PrintText(line++, "World position for Earth =         (% .5f, % .5f, % .5f% .5f)", earthWPos[0], earthWPos[1], earthWPos[2], earthWPos[3]);
			PrintText(line++, "Position for Earth =         (% .5f, % .5f, % .5f% .5f)", earthPos[0], earthPos[1], earthPos[2], earthPos[3]);
			
			//glm::mat4 testVM = setNewViewMatrix(_mainCamera, scene(), );

			 //View Matrix print
			/*
			glm::mat4 testVM = setNewViewMatrix(_mainCamera, scene(), scene()->sceneGraphNode("PlutoProjection"));
			
			PrintText(line++, "New Matrix: R 1 =         (% .5f, % .5f, % .5f% .5f)", testVM[0][0], testVM[0][1], testVM[0][2], testVM[0][3]);
			PrintText(line++, "New Matrix: R 2 =         (% .5f, % .5f, % .5f% .5f)", testVM[1][0], testVM[1][1], testVM[1][2], testVM[1][3]);
			PrintText(line++, "New Matrix: R 3 =         (% .5f, % .5f, % .5f% .5f)", testVM[2][0], testVM[2][1], testVM[2][2], testVM[2][3]);
			PrintText(line++, "New Matrix: R 4 =         (% .5f, % .5f, % .5f% .5f)", testVM[3][0], testVM[3][1], testVM[3][2], testVM[3][3]);
			*/
			//std::string commonParentNodename = findCommonParentNode(_mainCamera->getParent(), "Hydra",scene())->name().c_str();
			//PrintText(line++, "commonParentNodename: (%s)", commonParentNodename.c_str());
			double distance = DistanceToObject::ref().distanceCalc(_mainCamera->position(), _mainCamera->focusPosition());
			PrintText(line++, "Distance to target: (% .5f)", distance);
			_nameOfScene = setScene(scene(), _mainCamera, _nameOfScene);
			



			//PrintText(line++, "Name of Current scene: (%s)", _nameOfScene.c_str());
			//mat4 viewMatrix = _sgctEngine->getActiveViewMatrix() * userMatrix;
			
			//std::string commonParentNodename = findCommonParentNode(_mainCamera->ParentName, "Hydra", scene())->name();
			//PrintText(line++, "commonParentNodename: (%s)", commonParentNodename.c_str());
			glm::mat4 VM = _mainCamera->viewMatrix();
			
			glm::mat4 MM = _mainCamera->viewRotationMatrix(); // _mainCamera->modelMatrix();
			
			glm::mat4 PM = _mainCamera->viewProjectionMatrix();
			PrintText(line++, "basicTest(1): (% .5f)", basicTest(2.0f));
			
			//glm::mat4 PM = _mainCamera->projectionMatrix();	
//			glm::mat4 current_matrix =
		
			
			/*
			PrintText(line++, "View Matrix 1 =         (% .5f, % .5f, % .5f% .5f)", viewMatrix[0], viewMatrix[1], viewMatrix[2], viewMatrix[3]);
			
			PrintText(line++, "View Matrix 1 =         (% .5f, % .5f, % .5f% .5f)", viewMatrix[0], viewMatrix[1], viewMatrix[2], viewMatrix[3]);
			PrintText(line++, "View Matrix =         (% .5f, % .5f, % .5f% .5f)", VM[0][0], VM[1], VM[2], VM[3]);
			PrintText(line++, "Model Matrix =         (% .5f, % .5f, % .5f% .5f)", MM[0], MM[1], MM[2], MM[3]);
			PrintText(line++, "Projection Matrix =         (% .5f, % .5f, % .5f% .5f)", PM[0], PM[1], PM[2], PM[3]);
			*/
			
			
		
			//}
			
			//if (distance < 10000.0)
			//PrintText(line++, "Distance to (% .5f) ", distance);
			//PrintText(line++, "position for tmpnode =         (% .5f, % .5f, % .5f)", objpos[0], objpos[1], objpos[2]);
			//PrintText(line++,"%s", currentScene);


			//PrintText(line++, "Drawtime:       %.5f", sgct::Engine::instance()->getDrawTime());
			//PrintText(line++, "Frametime:      %.5f", sgct::Engine::instance()->getDt());
			//PrintText(i++, "Origin:         (% .5f, % .5f, % .5f, % .5f)", origin[0], origin[1], origin[2], origin[3]);
			//PrintText(i++, "Cam pos:        (% .5f, % .5f, % .5f, % .5f)", position[0], position[1], position[2], position[3]);
			//PrintText(i++, "View dir:       (% .5f, % .5f, % .5f)", viewdirection[0], viewdirection[1], viewdirection[2]);
			//PrintText(i++, "Cam->origin:    (% .15f, % .4f)", pssl[0], pssl[1]);
			//PrintText(i++, "Scaling:        (% .5f, % .5f)", scaling[0], scaling[1]);

#ifdef OPENSPACE_MODULE_NEWHORIZONS_ENABLED
			if (openspace::ImageSequencer2::ref().isReady()) {
				double remaining = openspace::ImageSequencer2::ref().getNextCaptureTime() - currentTime;
				double t = 1.0 - remaining / openspace::ImageSequencer2::ref().getIntervalLength();
				std::string progress = "|";
				int g = static_cast<int>((t* 24) + 1);
				g = std::max(g, 0);
				for (int i = 0; i < g; i++)
                          progress.append("-");
                      progress.append(">");
				for (int i = 0; i < 25 - g; i++)
                          progress.append(" ");

				std::string str = "";
				openspace::SpiceManager::ref().getDateFromET(openspace::ImageSequencer2::ref().getNextCaptureTime(), str, "YYYY MON DD HR:MN:SC");

				glm::vec4 active(0.6, 1, 0.00, 1);
				glm::vec4 brigther_active(0.9, 1, 0.75, 1);

				progress.append("|");
				if (remaining > 0) {
					brigther_active *= (1 - t);
					PrintColorText(line++, "Next instrument activity:", 10, active*t + brigther_active);
					PrintColorTextArg(line++, "%.0f s %s %.1f %%", 10, active*t + brigther_active, remaining, progress.c_str(), t * 100);
					PrintColorTextArg(line++, "Data acquisition time: %s", 10, active, str.c_str());
				}
				std::pair<double, std::string> nextTarget = ImageSequencer2::ref().getNextTarget();
   				std::pair<double, std::string> currentTarget = ImageSequencer2::ref().getCurrentTarget();

                if (currentTarget.first > 0.0) {
					int timeleft = static_cast<int>(nextTarget.first - currentTime);

					int hour   = timeleft / 3600;
					int second = timeleft % 3600;
					int minute = second / 60;
					    second = second % 60;

					std::string hh, mm, ss, coundtown;

					if (hour   < 10) hh.append("0");
					if (minute < 10) mm.append("0");
					if (second < 10) ss.append("0");

					hh.append(std::to_string(hour));
					mm.append(std::to_string(minute));
					ss.append(std::to_string(second));

					PrintColorTextArg(line++, "Data acquisition adjacency: [%s:%s:%s]", 10, targetColor, hh.c_str(), mm.c_str(), ss.c_str());
						
					std::pair<double, std::vector<std::string>> incidentTargets = ImageSequencer2::ref().getIncidentTargetList(2);
					std::string space;
					glm::vec4 color;
					size_t isize = incidentTargets.second.size(); 
                    for (size_t p = 0; p < isize; p++){
                        double t = static_cast<double>(p + 1) / static_cast<double>(isize + 1);
                        t = (p > isize / 2) ? 1 - t : t;
                        t += 0.3;
						color = (p == isize / 2) ? targetColor : glm::vec4(t, t, t, 1);
                        PrintColorTextArg(line, "%s%s", 10, color, space.c_str(), incidentTargets.second[p].c_str());
						for (int k = 0; k < incidentTargets.second[p].size() + 2; k++)
                            space += " ";
					}
					line++;
					
					std::map<std::string, bool> activeMap = ImageSequencer2::ref().getActiveInstruments();
					glm::vec4 firing(0.58-t, 1-t, 1-t, 1);
					glm::vec4 notFiring(0.5, 0.5, 0.5, 1);
					PrintColorText(line++, "Active Instruments: ", 10, active);
					for (auto t : activeMap){
						if (t.second == false){
							PrintColorText(line, "| |", 10, glm::vec4(0.3, 0.3, 0.3, 1));
							PrintColorTextArg(line++, "    %5s", 10, glm::vec4(0.3, 0.3, 0.3, 1), t.first.c_str());
						}
						else{
							PrintColorText(line, "|", 10, glm::vec4(0.3, 0.3, 0.3, 1));
							if (t.first == "NH_LORRI"){
								PrintColorText(line, " + ", 10, firing);
							}
							PrintColorText(line, "  |", 10, glm::vec4(0.3, 0.3, 0.3, 1));
							PrintColorTextArg(line++, "    %5s", 10, active, t.first.c_str());
						}
					}
                }
			}
#endif

#undef PrintText
			
		}

		if (_showScreenLog)
		{
			const sgct_text::Font* font = fontLight;
			const int max = 10;
			const int category_length = 20;
			const int msg_length = 140;
			const float ttl = 15.f;
			const float fade = 5.f;
			auto entries = _log->last(max);

			const glm::vec4 white(0.9, 0.9, 0.9, 1);
			const glm::vec4 red(1, 0, 0, 1);
			const glm::vec4 yellow(1, 1, 0, 1);
			const glm::vec4 green(0, 1, 0, 1);
			const glm::vec4 blue(0, 0, 1, 1);

			size_t nr = 1;
			for (auto& it = entries.first; it != entries.second; ++it) {
				const ScreenLog::LogEntry* e = &(*it);

				const double t = sgct::Engine::instance()->getTime();
				float diff = static_cast<float>(t - e->timeStamp);

				// Since all log entries are ordered, once one is exceeding TTL, all have
				if (diff > ttl)
					break;

				float alpha = 1;
				float ttf = ttl - fade;
				if (diff > ttf) {
					diff = diff - ttf;
					float p = 0.8f - diff / fade;
					alpha = (p <= 0.f) ? 0.f : pow(p, 0.3f);
				}

				// Since all log entries are ordered, once one exceeds alpha, all have
				if (alpha <= 0.0)
					break;

				const std::string lvl = "(" + ghoul::logging::LogManager::stringFromLevel(e->level) + ")";
				const std::string& message = e->message.substr(0, msg_length);
				nr += std::count(message.begin(), message.end(), '\n');

				Freetype::print(font, 10.f, static_cast<float>(font_size_light * nr * 2), white*alpha,
					"%-14s %s%s",									// Format
					e->timeString.c_str(),							// Time string
					e->category.substr(0, category_length).c_str(), // Category string (up to category_length)
					e->category.length() > 20 ? "..." : "");		// Pad category with "..." if exceeds category_length

				glm::vec4 color = white;
				if (e->level == ghoul::logging::LogManager::LogLevel::Debug)
					color = green;
				if (e->level == ghoul::logging::LogManager::LogLevel::Warning)
					color = yellow;
				if (e->level == ghoul::logging::LogManager::LogLevel::Error)
					color = red;
				if (e->level == ghoul::logging::LogManager::LogLevel::Fatal)
					color = blue;

				Freetype::print(font, static_cast<float>(10 + 39 * font_with_light), static_cast<float>(font_size_light * nr * 2), color*alpha, "%s", lvl.c_str());


				Freetype::print(font, static_cast<float>(10 + 53 * font_with_light), static_cast<float>(font_size_light * nr * 2), white*alpha, "%s", message.c_str());
				++nr;
			}
		}
	}
#endif
}

void RenderEngine::postDraw() {
    if (Time::ref().timeJumped())
        Time::ref().setTimeJumped(false);
	if (_takeScreenshot) {
		sgct::Engine::instance()->takeScreenshot();
		_takeScreenshot = false;
	}

	if (_doPerformanceMeasurements)
		storePerformanceMeasurements();
}

void RenderEngine::takeScreenshot() {
	_takeScreenshot = true;
}

void RenderEngine::toggleVisualizeABuffer(bool b) {
	_visualizeABuffer = b;
	if (!_visualizeABuffer)
		return;

	std::vector<ABuffer::fragmentData> _d = _abuffer->pixelData();
	_visualizer->updateData(_d);
}


void RenderEngine::toggleInfoText(bool b) {
	_showInfo = b;
}

Scene* RenderEngine::scene() {
	// TODO custom assert (ticket #5)
	assert(_sceneGraph);
	return _sceneGraph;
}

void RenderEngine::setSceneGraph(Scene* sceneGraph) {
	_sceneGraph = sceneGraph;
}

void RenderEngine::serialize(SyncBuffer* syncBuffer) {
	if (_mainCamera){
		_mainCamera->serialize(syncBuffer);
	}


    syncBuffer->encode(_onScreenInformation._node);
    syncBuffer->encode(_onScreenInformation._position.x);
    syncBuffer->encode(_onScreenInformation._position.y);
    syncBuffer->encode(_onScreenInformation._size);
}

void RenderEngine::deserialize(SyncBuffer* syncBuffer) {
	if (_mainCamera){
		_mainCamera->deserialize(syncBuffer);
	}
    syncBuffer->decode(_onScreenInformation._node);
    syncBuffer->decode(_onScreenInformation._position.x);
    syncBuffer->decode(_onScreenInformation._position.y);
    syncBuffer->decode(_onScreenInformation._size);

}

Camera* RenderEngine::camera() const {
	return _mainCamera;
}

ABuffer* RenderEngine::aBuffer() const {
	return _abuffer;
}

RenderEngine::ABufferImplementation RenderEngine::aBufferImplementation() const {
    return _abufferImplementation;
}

float RenderEngine::globalBlackOutFactor() {
	return _globalBlackOutFactor;
}

void RenderEngine::setGlobalBlackOutFactor(float opacity) {
	_globalBlackOutFactor = opacity;
}

void RenderEngine::startFading(int direction, float fadeDuration) {
	_fadeDirection = direction;
	_fadeDuration = fadeDuration;
	_currentFadeTime = 0.f;
}

void RenderEngine::generateGlslConfig() {
    ghoul_assert(_abuffer != nullptr, "ABuffer not initialized");
	LDEBUG("Generating GLSLS config, expect shader recompilation");
	int xSize = sgct::Engine::instance()->getActiveWindowPtr()->getXFramebufferResolution();;
	int ySize = sgct::Engine::instance()->getActiveWindowPtr()->getYFramebufferResolution();;

	// TODO: Make this file creation dynamic and better in every way
	// TODO: If the screen size changes it is enough if this file is regenerated to
	// recompile all necessary files
	std::ofstream os(absPath("${SHADERS_GENERATED}/constants.hglsl"));
	os << "#ifndef CONSTANTS_HGLSL\n"
		<< "#define CONSTANTS_HGLSL\n"
		<< "#define SCREEN_WIDTH  " << xSize << "\n"
		<< "#define SCREEN_HEIGHT " << ySize << "\n"
		<< "#define MAX_LAYERS " << ABuffer::MAX_LAYERS << "\n"
		<< "#define ABUFFER_FRAMEBUFFER       " << ABUFFER_FRAMEBUFFER << "\n"
		<< "#define ABUFFER_SINGLE_LINKED     " << ABUFFER_SINGLE_LINKED << "\n"
		<< "#define ABUFFER_FIXED             " << ABUFFER_FIXED << "\n"
		<< "#define ABUFFER_DYNAMIC           " << ABUFFER_DYNAMIC << "\n"
		<< "#define ABUFFER_IMPLEMENTATION    " << int(_abufferImplementation) << "\n";
	// System specific
#ifdef WIN32
	os << "#define WIN32\n";
#endif
#ifdef __APPLE__
	os << "#define APPLE\n";
#endif
#ifdef __linux__
	os << "#define linux\n";
#endif
	os << "#endif\n";

	os.close();
}

scripting::ScriptEngine::LuaLibrary RenderEngine::luaLibrary() {
	return {
		"",
		{
			{
				"takeScreenshot",
				&luascriptfunctions::takeScreenshot,
				"",
				"Renders the current image to a file on disk"
			},
			{
				"visualizeABuffer",
				&luascriptfunctions::visualizeABuffer,
				"bool",
				"Toggles the visualization of the ABuffer",
                true
			},
			{
				"showRenderInformation",
				&luascriptfunctions::showRenderInformation,
				"bool",
				"Toggles the showing of render information on-screen text"
			},
            {
                "showSGCTRenderStatistics",
                &luascriptfunctions::showSGCTRenderStatistics,
                "bool",
                "Toggles the visibility of the SGCT rendering information"
            },
			{
				"setPerformanceMeasurement",
				&luascriptfunctions::setPerformanceMeasurement,
				"bool",
				"Sets the performance measurements"
			},
		    {
			    "fadeIn",
			    &luascriptfunctions::fadeIn,
			    "number",
			    "",
                true
		    },
		    //also temporary @JK
		    {
			    "fadeOut",
			    &luascriptfunctions::fadeOut,
			    "number",
			    "",
                true
		    },
		},
	};
}

void RenderEngine::setPerformanceMeasurements(bool performanceMeasurements) {
	_doPerformanceMeasurements = performanceMeasurements;
}

bool RenderEngine::doesPerformanceMeasurements() const {
	return _doPerformanceMeasurements;
}

void findCommonParent(){
	 

}

void RenderEngine::camerRigTesting() {
	LINFO("CameraRig");
//Write for psc and without
//Check camera what matrixes it gets and from where.
	//Traversal fetching from all parents:
	//need to traverse the scene tree for all the viewMatrices.
	
	//--------------- stuff---------- origin for camera change

/*
	________________________
    \                       \
	 \						 \
	  \                       \
	   ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
						     
*/
	glm::mat4 VM = _mainCamera->viewMatrix();
	//SceneGraphNode(_mainCamera->ParentName)
	SceneGraphNode* camParent = scene()->sceneGraphNode(_mainCamera->getParent());
	//glm::vec3 origoToCamVec = _mainCamera->position().vec3();
	std::vector<SceneGraphNode*> path = pathTo(camParent);



	//How to find matrix of parent?
	//Create dummyCam instance, move to parent with current ofset and collect matrices?

	//need to traverse the scene tree for all the modelMatrices.
	glm::mat4 MM = _mainCamera->modelMatrix();
	glm::mat4 newViewMatrix = VM;



	//Scaling, a float value, collects the scale between all 

//Only need one
	//fetch proj matrix (only need one?) 
	glm::mat4 PM = _mainCamera->projectionMatrix();

// find out whos the common parent
	

	/*
//				   Root
					
					||
				Solarsystem
					||
					/\
				   /  \
				  /\
				 / /\
				/	 \
			   /	  Obj
			 Cam




*/
}


void RenderEngine::storePerformanceMeasurements() {
	const int8_t Version = 0;
	const int nValues = 250;
	const int lengthName = 256;
	const int maxValues = 256;

	struct PerformanceLayout {
		int8_t version;
		int32_t nValuesPerEntry;
		int32_t nEntries;
		int32_t maxNameLength;
		int32_t maxEntries;

		struct PerformanceLayoutEntry {
			char name[lengthName];
			float renderTime[nValues];
			float updateRenderable[nValues];
			float updateEphemeris[nValues];

			int32_t currentRenderTime;
			int32_t currentUpdateRenderable;
			int32_t currentUpdateEphemeris;
		};

		PerformanceLayoutEntry entries[maxValues];
	};

	const int nNodes = static_cast<int>(scene()->allSceneGraphNodes().size());
	if (!_performanceMemory) {

		// Compute the total size
		const int totalSize = sizeof(int8_t) + 4 * sizeof(int32_t) +
			maxValues * sizeof(PerformanceLayout::PerformanceLayoutEntry);
		LINFO("Create shared memory of " << totalSize << " bytes");

		ghoul::SharedMemory::create(PerformanceMeasurementSharedData, totalSize);
		_performanceMemory = new ghoul::SharedMemory(PerformanceMeasurementSharedData);

		PerformanceLayout* layout = reinterpret_cast<PerformanceLayout*>(_performanceMemory->pointer());
		layout->version = Version;
		layout->nValuesPerEntry = nValues;
		layout->nEntries = nNodes;
		layout->maxNameLength = lengthName;
		layout->maxEntries = maxValues;

		memset(layout->entries, 0, maxValues * sizeof(PerformanceLayout::PerformanceLayoutEntry));

		for (int i = 0; i < nNodes; ++i) {
			SceneGraphNode* node = scene()->allSceneGraphNodes()[i];

			memset(layout->entries[i].name, 0, lengthName);
#ifdef _MSC_VER
            strcpy_s(layout->entries[i].name, node->name().length() + 1, node->name().c_str());
#else
            strcpy(layout->entries[i].name, node->name().c_str());
#endif

			layout->entries[i].currentRenderTime = 0;
			layout->entries[i].currentUpdateRenderable = 0;
			layout->entries[i].currentUpdateEphemeris = 0;
		}
	}

	PerformanceLayout* layout = reinterpret_cast<PerformanceLayout*>(_performanceMemory->pointer());
	_performanceMemory->acquireLock();
	for (int i = 0; i < nNodes; ++i) {
		SceneGraphNode* node = scene()->allSceneGraphNodes()[i];
		SceneGraphNode::PerformanceRecord r = node->performanceRecord();
		PerformanceLayout::PerformanceLayoutEntry& entry = layout->entries[i];

		entry.renderTime[entry.currentRenderTime] = r.renderTime / 1000.f;
		entry.updateEphemeris[entry.currentUpdateEphemeris] = r.updateTimeEphemeris / 1000.f;
		entry.updateRenderable[entry.currentUpdateRenderable] = r.updateTimeRenderable / 1000.f;

		entry.currentRenderTime = (entry.currentRenderTime + 1) % nValues;
		entry.currentUpdateEphemeris = (entry.currentUpdateEphemeris + 1) % nValues;
		entry.currentUpdateRenderable = (entry.currentUpdateRenderable + 1) % nValues;
	}
	_performanceMemory->releaseLock();
}

// This method is temporary and will be removed once the scalegraph is in effect ---abock
void RenderEngine::changeViewPoint(std::string origin) {
    SceneGraphNode* solarSystemBarycenterNode = scene()->sceneGraphNode("SolarSystemBarycenter");
    SceneGraphNode* plutoBarycenterNode = scene()->sceneGraphNode("PlutoBarycenter");
    SceneGraphNode* newHorizonsNode = scene()->sceneGraphNode("NewHorizons");
	SceneGraphNode* newHorizonsPathNodeJ = scene()->sceneGraphNode("NewHorizonsPathJupiter");
	SceneGraphNode* newHorizonsPathNodeP = scene()->sceneGraphNode("NewHorizonsPathPluto");
	RenderablePath* nhPath;

    SceneGraphNode* jupiterBarycenterNode = scene()->sceneGraphNode("JupiterBarycenter");

	//SceneGraphNode* newHorizonsGhostNode = scene()->sceneGraphNode("NewHorizonsGhost");
	//SceneGraphNode* dawnNode = scene()->sceneGraphNode("Dawn");
	//SceneGraphNode* vestaNode = scene()->sceneGraphNode("Vesta");

    if (solarSystemBarycenterNode == nullptr || plutoBarycenterNode == nullptr || 
		newHorizonsNode == nullptr || jupiterBarycenterNode == nullptr 
		//||	dawnNode == nullptr 
		//||  vestaNode == nullptr
		) {
	    LERROR("Necessary nodes does not exist");
		return;
    }

    if (origin == "Pluto") {
		if (newHorizonsPathNodeP) {
			Renderable* R = newHorizonsPathNodeP->renderable();
			newHorizonsPathNodeP->setParent(plutoBarycenterNode);
			nhPath = static_cast<RenderablePath*>(R);
			nhPath->calculatePath("PLUTO BARYCENTER");
		}

		plutoBarycenterNode->setParent(scene()->sceneGraphNode("SolarSystem"));
		plutoBarycenterNode->setEphemeris(new StaticEphemeris);
		
		solarSystemBarycenterNode->setParent(plutoBarycenterNode);
		newHorizonsNode->setParent(plutoBarycenterNode);
		//newHorizonsGhostNode->setParent(plutoBarycenterNode);

		//dawnNode->setParent(plutoBarycenterNode);
		//vestaNode->setParent(plutoBarycenterNode);

		//newHorizonsTrailNode->setParent(plutoBarycenterNode);

	

		ghoul::Dictionary solarDictionary =
		{
			{ std::string("Type"), std::string("Spice") },
			{ std::string("Body"), std::string("SUN") },
			{ std::string("Reference"), std::string("GALACTIC") },
			{ std::string("Observer"), std::string("PLUTO BARYCENTER") },
			{ std::string("Kernels"), ghoul::Dictionary() }
		};
        
        ghoul::Dictionary jupiterDictionary =
        {
            { std::string("Type"), std::string("Spice") },
            { std::string("Body"), std::string("JUPITER BARYCENTER") },
            { std::string("Reference"), std::string("GALACTIC") },
            { std::string("Observer"), std::string("PLUTO BARYCENTER") },
            { std::string("Kernels"), ghoul::Dictionary() }
        };

        ghoul::Dictionary newHorizonsDictionary =
        {
            { std::string("Type"), std::string("Spice") },
            { std::string("Body"), std::string("NEW HORIZONS") },
            { std::string("Reference"), std::string("GALACTIC") },
            { std::string("Observer"), std::string("PLUTO BARYCENTER") },
            { std::string("Kernels"), ghoul::Dictionary() }
        };

		solarSystemBarycenterNode->setEphemeris(new SpiceEphemeris(solarDictionary));
		jupiterBarycenterNode->setEphemeris(new SpiceEphemeris(jupiterDictionary));
        newHorizonsNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));
		//newHorizonsTrailNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));


		//ghoul::Dictionary dawnDictionary =
        //{
        //    { std::string("Type"), std::string("Spice") },
        //    { std::string("Body"), std::string("DAWN") },
        //    { std::string("Reference"), std::string("GALACTIC") },
        //    { std::string("Observer"), std::string("PLUTO BARYCENTER") },
        //    { std::string("Kernels"), ghoul::Dictionary() }
        //};
        //dawnNode->setEphemeris(new SpiceEphemeris(dawnDictionary));
		//
		//ghoul::Dictionary vestaDictionary =
		//{
		//	  { std::string("Type"), std::string("Spice") },
		//	  { std::string("Body"), std::string("VESTA") },
		//	  { std::string("Reference"), std::string("GALACTIC") },
		//	  { std::string("Observer"), std::string("PLUTO BARYCENTER") },
		//	  { std::string("Kernels"), ghoul::Dictionary() }
		//};
		//vestaNode->setEphemeris(new SpiceEphemeris(vestaDictionary));

		
		//ghoul::Dictionary newHorizonsGhostDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("NEW HORIZONS") },
		//	{ std::string("EphmerisGhosting"), std::string("TRUE") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("PLUTO BARYCENTER") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//newHorizonsGhostNode->setEphemeris(new SpiceEphemeris(newHorizonsGhostDictionary));
		
        return;
    }
    if (origin == "Sun") {
		solarSystemBarycenterNode->setParent(scene()->sceneGraphNode("SolarSystem"));

		plutoBarycenterNode->setParent(solarSystemBarycenterNode);
		jupiterBarycenterNode->setParent(solarSystemBarycenterNode);
		newHorizonsNode->setParent(solarSystemBarycenterNode);
		//newHorizonsGhostNode->setParent(solarSystemBarycenterNode);

		//newHorizonsTrailNode->setParent(solarSystemBarycenterNode);
		//dawnNode->setParent(solarSystemBarycenterNode);
		//vestaNode->setParent(solarSystemBarycenterNode);

        ghoul::Dictionary plutoDictionary =
        {
            { std::string("Type"), std::string("Spice") },
            { std::string("Body"), std::string("PLUTO BARYCENTER") },
            { std::string("Reference"), std::string("GALACTIC") },
            { std::string("Observer"), std::string("SUN") },
            { std::string("Kernels"), ghoul::Dictionary() }
        };
        ghoul::Dictionary jupiterDictionary =
        {
            { std::string("Type"), std::string("Spice") },
            { std::string("Body"), std::string("JUPITER BARYCENTER") },
            { std::string("Reference"), std::string("GALACTIC") },
            { std::string("Observer"), std::string("SUN") },
            { std::string("Kernels"), ghoul::Dictionary() }
        };
        
        solarSystemBarycenterNode->setEphemeris(new StaticEphemeris);
        jupiterBarycenterNode->setEphemeris(new SpiceEphemeris(jupiterDictionary));
        plutoBarycenterNode->setEphemeris(new SpiceEphemeris(plutoDictionary));

        ghoul::Dictionary newHorizonsDictionary =
        {
            { std::string("Type"), std::string("Spice") },
            { std::string("Body"), std::string("NEW HORIZONS") },
            { std::string("Reference"), std::string("GALACTIC") },
            { std::string("Observer"), std::string("SUN") },
            { std::string("Kernels"), ghoul::Dictionary() }
        };
        newHorizonsNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));
		//newHorizonsTrailNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));

        
		//ghoul::Dictionary dawnDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("DAWN") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("SUN") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//dawnNode->setEphemeris(new SpiceEphemeris(dawnDictionary));
		//
		//ghoul::Dictionary vestaDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("VESTA") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("SUN") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//vestaNode->setEphemeris(new SpiceEphemeris(vestaDictionary));
		
		
		//ghoul::Dictionary newHorizonsGhostDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("NEW HORIZONS") },
		//	{ std::string("EphmerisGhosting"), std::string("TRUE") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//newHorizonsGhostNode->setEphemeris(new SpiceEphemeris(newHorizonsGhostDictionary));
		
        return;
    }
    if (origin == "Jupiter") {
		if (newHorizonsPathNodeJ) {
			Renderable* R = newHorizonsPathNodeJ->renderable();
			newHorizonsPathNodeJ->setParent(jupiterBarycenterNode);
			nhPath = static_cast<RenderablePath*>(R);
			nhPath->calculatePath("JUPITER BARYCENTER");
		}

		jupiterBarycenterNode->setParent(scene()->sceneGraphNode("SolarSystem"));
		jupiterBarycenterNode->setEphemeris(new StaticEphemeris);

		solarSystemBarycenterNode->setParent(jupiterBarycenterNode);
		newHorizonsNode->setParent(jupiterBarycenterNode);
		//newHorizonsTrailNode->setParent(jupiterBarycenterNode);

		//dawnNode->setParent(jupiterBarycenterNode);
		//vestaNode->setParent(jupiterBarycenterNode);


		ghoul::Dictionary solarDictionary =
		{
			{ std::string("Type"), std::string("Spice") },
			{ std::string("Body"), std::string("SUN") },
			{ std::string("Reference"), std::string("GALACTIC") },
			{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
			{ std::string("Kernels"), ghoul::Dictionary() }
		};

		ghoul::Dictionary plutoDictionary =
		{
			{ std::string("Type"), std::string("Spice") },
			{ std::string("Body"), std::string("PlUTO BARYCENTER") },
			{ std::string("Reference"), std::string("GALACTIC") },
			{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
			{ std::string("Kernels"), ghoul::Dictionary() }
		};

		ghoul::Dictionary newHorizonsDictionary =
		{
			{ std::string("Type"), std::string("Spice") },
			{ std::string("Body"), std::string("NEW HORIZONS") },
			{ std::string("Reference"), std::string("GALACTIC") },
			{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
			{ std::string("Kernels"), ghoul::Dictionary() }
		};
		solarSystemBarycenterNode->setEphemeris(new SpiceEphemeris(solarDictionary));
		plutoBarycenterNode->setEphemeris(new SpiceEphemeris(plutoDictionary));
		newHorizonsNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));
		//newHorizonsGhostNode->setParent(jupiterBarycenterNode);
		//newHorizonsTrailNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));


		//ghoul::Dictionary dawnDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("DAWN") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//dawnNode->setEphemeris(new SpiceEphemeris(dawnDictionary));
		//
		//ghoul::Dictionary vestaDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("VESTA") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//vestaNode->setEphemeris(new SpiceEphemeris(vestaDictionary));


		
		//ghoul::Dictionary newHorizonsGhostDictionary =
		//{
		//	{ std::string("Type"), std::string("Spice") },
		//	{ std::string("Body"), std::string("NEW HORIZONS") },
		//	{ std::string("EphmerisGhosting"), std::string("TRUE") },
		//	{ std::string("Reference"), std::string("GALACTIC") },
		//	{ std::string("Observer"), std::string("JUPITER BARYCENTER") },
		//	{ std::string("Kernels"), ghoul::Dictionary() }
		//};
		//newHorizonsGhostNode->setEphemeris(new SpiceEphemeris(newHorizonsGhostDictionary));
		//newHorizonsGhostNode->setParent(jupiterBarycenterNode);

	
        return;
    }
	//if (origin == "Vesta") {
	//	
	//	vestaNode->setParent(scene()->sceneGraphNode("SolarSystem"));
	//	vestaNode->setEphemeris(new StaticEphemeris);
	//
	//	solarSystemBarycenterNode->setParent(vestaNode);
	//	newHorizonsNode->setParent(vestaNode);
	//
	//	dawnNode->setParent(vestaNode);
	//	plutoBarycenterNode->setParent(vestaNode);
	//
	//
	//	ghoul::Dictionary plutoDictionary =
	//	{
	//		{ std::string("Type"), std::string("Spice") },
	//		{ std::string("Body"), std::string("PLUTO BARYCENTER") },
	//		{ std::string("Reference"), std::string("GALACTIC") },
	//		{ std::string("Observer"), std::string("VESTA") },
	//		{ std::string("Kernels"), ghoul::Dictionary() }
	//	};
	//	ghoul::Dictionary solarDictionary =
	//	{
	//		{ std::string("Type"), std::string("Spice") },
	//		{ std::string("Body"), std::string("SUN") },
	//		{ std::string("Reference"), std::string("GALACTIC") },
	//		{ std::string("Observer"), std::string("VESTA") },
	//		{ std::string("Kernels"), ghoul::Dictionary() }
	//	};
	//
	//	ghoul::Dictionary jupiterDictionary =
	//	{
	//		{ std::string("Type"), std::string("Spice") },
	//		{ std::string("Body"), std::string("JUPITER BARYCENTER") },
	//		{ std::string("Reference"), std::string("GALACTIC") },
	//		{ std::string("Observer"), std::string("VESTA") },
	//		{ std::string("Kernels"), ghoul::Dictionary() }
	//	};
	//
	//	solarSystemBarycenterNode->setEphemeris(new SpiceEphemeris(solarDictionary));
	//	plutoBarycenterNode->setEphemeris(new SpiceEphemeris(plutoDictionary));
	//	jupiterBarycenterNode->setEphemeris(new SpiceEphemeris(jupiterDictionary));
	//
	//	ghoul::Dictionary newHorizonsDictionary =
	//	{
	//		{ std::string("Type"), std::string("Spice") },
	//		{ std::string("Body"), std::string("NEW HORIZONS") },
	//		{ std::string("Reference"), std::string("GALACTIC") },
	//		{ std::string("Observer"), std::string("VESTA") },
	//		{ std::string("Kernels"), ghoul::Dictionary() }
	//	};
	//	newHorizonsNode->setEphemeris(new SpiceEphemeris(newHorizonsDictionary));
	//
	//	ghoul::Dictionary dawnDictionary =
	//	{
	//		{ std::string("Type"), std::string("Spice") },
	//		{ std::string("Body"), std::string("DAWN") },
	//		{ std::string("Reference"), std::string("GALACTIC") },
	//		{ std::string("Observer"), std::string("VESTA") },
	//		{ std::string("Kernels"), ghoul::Dictionary() }
	//	};
	//	dawnNode->setEphemeris(new SpiceEphemeris(dawnDictionary));
	//	vestaNode->setEphemeris(new StaticEphemeris);
	//
	//	return;
	//}

	//if (origin == "67P") {
	//	SceneGraphNode* rosettaNode = scene()->sceneGraphNode("Rosetta");
	//	SceneGraphNode* cgNode = scene()->sceneGraphNode("67P");
	//	//jupiterBarycenterNode->setParent(solarSystemBarycenterNode);
	//	//plutoBarycenterNode->setParent(solarSystemBarycenterNode);
	//	solarSystemBarycenterNode->setParent(cgNode);
	//	rosettaNode->setParent(cgNode);
	//	
	//	ghoul::Dictionary solarDictionary =
	//		{
	//		{ std::string("Type"), std::string("Spice") },
	//			{ std::string("Body"), std::string("SUN") },
	//			{ std::string("Reference"), std::string("GALACTIC") },
	//			{ std::string("Observer"), std::string("CHURYUMOV-GERASIMENKO") },
	//			{ std::string("Kernels"), ghoul::Dictionary() }
	//		};
	//	solarSystemBarycenterNode->setEphemeris(new SpiceEphemeris(solarDictionary));
	//	
	//	ghoul::Dictionary rosettaDictionary =
	//		{
	//		{ std::string("Type"), std::string("Spice") },
	//			{ std::string("Body"), std::string("ROSETTA") },
	//			{ std::string("Reference"), std::string("GALACTIC") },
	//			{ std::string("Observer"), std::string("CHURYUMOV-GERASIMENKO") },
	//			{ std::string("Kernels"), ghoul::Dictionary() }
	//		};
	//	
	//	cgNode->setParent(scene()->sceneGraphNode("SolarSystem"));
	//	rosettaNode->setEphemeris(new SpiceEphemeris(rosettaDictionary));
	//	cgNode->setEphemeris(new StaticEphemeris);
	//	
	//	return;
	//	
	//}

    LFATAL("This function is being misused with an argument of '" << origin << "'");
}

void RenderEngine::setSGCTRenderStatistics(bool visible) {
    _sgctRenderStatisticsVisible = visible;
}

void RenderEngine::setDisableRenderingOnMaster(bool enabled) {
    _disableMasterRendering = enabled;
}

RenderEngine::ABufferImplementation RenderEngine::aBufferFromString(const std::string& impl) {
    const std::map<std::string, RenderEngine::ABufferImplementation> RenderingMethods = {
        { "ABufferFrameBuffer", ABufferImplementation::FrameBuffer },
        { "ABufferSingleLinked", ABufferImplementation::SingleLinked },
        { "ABufferFixed", ABufferImplementation::Fixed },
        { "ABufferDynamic", ABufferImplementation::Dynamic }
    };

    if (RenderingMethods.find(impl) != RenderingMethods.end())
        return RenderingMethods.at(impl);
    else
        return ABufferImplementation::Invalid;
}

}// namespace openspace
