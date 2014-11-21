/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
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

#include <openspace/rendering/renderableflare.h>
#include <openspace/rendering/flare/tsp.h>
#include <openspace/rendering/flare/brickmanager.h>

// ghoul includes
#include <ghoul/opengl/framebufferobject.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/texturereader.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

// other
#include <sgct.h>

using ghoul::opengl::FramebufferObject;
using ghoul::opengl::Texture;

namespace {
	const std::string _loggerCat = "RenderableFlare";

	const std::string keyDataSource = "Source";
	const std::string keyTransferFunction = "TransferFunction";
	const std::string keyWorksizeX = "local_worksize_x";
	const std::string keyWorksizeY = "local_worksize_y";
	const std::string keyTraversalStepsize = "tsp_traveral_stepsize";
	const std::string keyRaycasterStepsize = "raycaster_stepsize";
	const std::string keyTSPTraversal = "TSPTraversal";
	const std::string keyRaycasterTSP = "raycasterTSP";
}

namespace openspace {

RenderableFlare::RenderableFlare(const ghoul::Dictionary& dictionary)
	: Renderable(dictionary)
	, _tsp(nullptr)
	, _brickManager(nullptr)
	, _boxArray(0)
	, dispatch_buffer(0)
	, _tspTraversal(nullptr)
	, _raycasterTsp(nullptr)
	, _cubeProgram(nullptr)
	, _textureToAbuffer(nullptr)
	, _fbo(nullptr)
	, _backTexture(nullptr)
	, _frontTexture(nullptr)
	, _transferFunction(nullptr)
{
	std::string s;
	dictionary.getValue(keyDataSource, s);
	s = absPath(s);
	if (!FileSys.fileExists(s, true))
		return;

	_tsp = new TSP(s);
	_brickManager = new BrickManager(s);

	std::string transferfunctionPath;
	if (dictionary.getValue(keyTransferFunction, transferfunctionPath)) {
		transferfunctionPath = findPath(transferfunctionPath);
		if (transferfunctionPath != "")
			_transferFunction = ghoul::opengl::loadTexture(transferfunctionPath);
		if (!_transferFunction)
			LERROR("Could not load transferfunction");
	}

	dictionary.getValue(keyTSPTraversal, _traversalPath);
	dictionary.getValue(keyRaycasterTSP, _raycasterPath);
	_traversalPath = findPath(_traversalPath);
	_raycasterPath = findPath(_raycasterPath);
	
}

RenderableFlare::~RenderableFlare() {
	if (_tsp)
		delete _tsp;
	if (_brickManager)
		delete _brickManager;
	if (dispatch_buffer)
		glDeleteBuffers(1, &dispatch_buffer);
	if (_boxArray)
		glDeleteVertexArrays(1, &_boxArray);
	if (_tspTraversal)
		delete _tspTraversal;
	if (_raycasterTsp)
		delete _raycasterTsp;
	if (_cubeProgram)
		delete _cubeProgram;
	if (_textureToAbuffer)
		delete _textureToAbuffer;
	if (_fbo)
		delete _fbo;
	if (_backTexture)
		delete _backTexture;
	if (_frontTexture)
		delete _frontTexture;
	if (_transferFunction)
		delete _transferFunction;
}

bool RenderableFlare::initialize() {
	bool success = true;

	if (_tsp) {
		success |= _tsp->load();
	}
	if (_brickManager) {
		success |= _brickManager->readHeader();
		success |= _brickManager->initialize();
	}
	
	if (success) {

		ghoul::opengl::ShaderObject* tspTraversalObject = new ghoul::opengl::ShaderObject(
			ghoul::opengl::ShaderObject::ShaderType::ShaderTypeCompute,
			_traversalPath,
			std::string("_tspTraversal CS") );

		_tspTraversal = new ghoul::opengl::ProgramObject("_tspTraversal");
		_tspTraversal->attachObject(tspTraversalObject);
		if (!_tspTraversal->compileShaderObjects())
			LERROR("Could not compile shader objects");
		if (!_tspTraversal->linkProgramObject())
			LERROR("Could not link shader objects");

		_tspTraversal->activate();

		static const struct
		{
			GLuint num_groups_x;
			GLuint num_groups_y;
			GLuint num_groups_z;
		} dispatch_params = { 1280 / 16, 720 / 16, 1 };
		glGenBuffers(1, &dispatch_buffer);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_buffer);
		glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof(dispatch_params), &dispatch_params, GL_STATIC_DRAW);
		_tspTraversal->deactivate();

		/*
		ghoul::opengl::ShaderObject* raycasterTSPObject = new ghoul::opengl::ShaderObject(
			ghoul::opengl::ShaderObject::ShaderType::ShaderTypeCompute,
			_raycasterPath,
			std::string("_tspTraversal CS"));

		_raycasterTsp = new ghoul::opengl::ProgramObject("_tspTraversal");
		_raycasterTsp->attachObject(raycasterTSPObject);
		if (!_raycasterTsp->compileShaderObjects())
			LERROR("Could not compile shader objects");
		if (!_raycasterTsp->linkProgramObject())
			LERROR("Could not link shader objects");

			_raycasterTsp->activate();

			static const struct
			{
			GLuint num_groups_x;
			GLuint num_groups_y;
			GLuint num_groups_z;
			} dispatch_params = { 1280 / 16, 720 / 16, 1 };
			glGenBuffers(1, &dispatch_buffer);
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_buffer);
			glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof(dispatch_params), &dispatch_params, GL_STATIC_DRAW);
			_raycasterTsp->deactivate();
		*/

		// ============================
		//      GEOMETRY (box)
		// ============================
		const GLfloat size = 0.5f;
		const GLfloat _w = 0.0f;
		const GLfloat vertex_data[] = {
			//  x,     y,     z,     s,
			-size, -size,  size,  _w,  0.0,  0.0,  1.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,
			-size,  size,  size,  _w,  0.0,  1.0,  1.0,  1.0,
			-size, -size,  size,  _w,  0.0,  0.0,  1.0,  1.0,
			 size, -size,  size,  _w,  1.0,  0.0,  1.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,

			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			 size,  size, -size,  _w,  1.0,  1.0,  0.0,  1.0,
			-size,  size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			 size, -size, -size,  _w,  1.0,  0.0,  0.0,  1.0,
			 size,  size, -size,  _w,  1.0,  1.0,  0.0,  1.0,

			 size, -size, -size,  _w,  1.0,  0.0,  0.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,
			 size, -size,  size,  _w,  1.0,  0.0,  1.0,  1.0,
			 size, -size, -size,  _w,  1.0,  0.0,  0.0,  1.0,
			 size,  size, -size,  _w,  1.0,  1.0,  0.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,

			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			-size,  size,  size,  _w,  0.0,  1.0,  1.0,  1.0,
			-size, -size,  size,  _w,  0.0,  0.0,  1.0,  1.0,
			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			-size,  size, -size,  _w,  0.0,  1.0,  0.0,  1.0,
			-size,  size,  size,  _w,  0.0,  1.0,  1.0,  1.0,

			-size,  size, -size,  _w,  0.0,  1.0,  0.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,
			-size,  size,  size,  _w,  0.0,  1.0,  1.0,  1.0,
			-size,  size, -size,  _w,  0.0,  1.0,  0.0,  1.0,
			 size,  size, -size,  _w,  1.0,  1.0,  0.0,  1.0,
			 size,  size,  size,  _w,  1.0,  1.0,  1.0,  1.0,

			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			 size, -size,  size,  _w,  1.0,  0.0,  1.0,  1.0,
			-size, -size,  size,  _w,  0.0,  0.0,  1.0,  1.0,
			-size, -size, -size,  _w,  0.0,  0.0,  0.0,  1.0,
			 size, -size, -size,  _w,  1.0,  0.0,  0.0,  1.0,
			 size, -size,  size,  _w,  1.0,  0.0,  1.0,  1.0,
		};

		GLuint vertexPositionBuffer;
		glGenVertexArrays(1, &_boxArray); // generate array
		glBindVertexArray(_boxArray); // bind array
		glGenBuffers(1, &vertexPositionBuffer); // generate buffer
		glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, reinterpret_cast<void*>(0));
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, reinterpret_cast<void*>(sizeof(GLfloat) * 4));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		_fbo = new FramebufferObject();

		int x1, xSize, y1, ySize;
		sgct::Engine::instance()->getActiveWindowPtr()->getCurrentViewportPixelCoords(x1, y1, xSize, ySize);
		size_t x = static_cast<size_t>(xSize);
		size_t y = static_cast<size_t>(ySize);

		_backTexture = new Texture(glm::size3_t(x, y, 1));
		_frontTexture = new Texture(glm::size3_t(x, y, 1));
		_backTexture->uploadTexture();
		_frontTexture->uploadTexture();
		_fbo->attachTexture(_backTexture, GL_COLOR_ATTACHMENT0);
		_fbo->attachTexture(_frontTexture, GL_COLOR_ATTACHMENT1);
		_fbo->deactivate();
	}

	return success;
}

bool RenderableFlare::deinitialize() {
	return true;
}

void RenderableFlare::render(const RenderData& data) {
	GLuint activeFBO = FramebufferObject::getActiveObject(); // Save SGCTs main FBO
	_fbo->activate();
	_boxProgram->activate();

	const Camera& camera = data.camera;
	const psc& position = data.position;
	setPscUniforms(_boxProgram, &camera, position);
	_boxProgram->setUniform("modelViewProjection", camera.projectionMatrix);
	_boxProgram->setUniform("modelTransform", glm::mat4(1.0));

	sgct_core::Frustum::FrustumMode mode = sgct::Engine::instance()->
		getActiveWindowPtr()->
		getCurrentViewport()->
		getEye();

	// oh god why..?
	if (mode == sgct_core::Frustum::FrustumMode::Mono ||
		mode == sgct_core::Frustum::FrustumMode::StereoLeftEye) {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

	}
	// make sure GL_CULL_FACE is enabled (it should be)
	glEnable(GL_CULL_FACE);

	//      Draw backface
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glCullFace(GL_FRONT);
	_boundingBox->draw();
	//      Draw frontface (now the normal cull face is is set)
	glDrawBuffer(GL_COLOR_ATTACHMENT1);
	glCullFace(GL_BACK);
	_boundingBox->draw();
	_boxProgram->deactivate();
	_fbo->deactivate();

	// rebind the previous FBO
	glBindFramebuffer(GL_FRAMEBUFFER, activeFBO);

	// Prepare positional data
	const Camera& camera = data.camera;
	const psc& position = data.position;
	setPscUniforms(_tspTraversal, &camera, position);

	sgct_core::Frustum::FrustumMode mode = sgct::Engine::instance()->
		getActiveWindowPtr()->
		getCurrentViewport()->
		getEye();

	// oh god why..?
	if (mode == sgct_core::Frustum::FrustumMode::Mono ||
		mode == sgct_core::Frustum::FrustumMode::StereoLeftEye) {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}


	// Render front texture
	_fbo->activate();

	// Render back texture


	// Dispatch TSP traversal
	//_tspTraversal->activate();
	//glDispatchComputeIndirect(0);
	//_tspTraversal->deactivate();


	// PBO to atlas

	// Dispatch Raycaster
	//_tspTraversal->activate();
	//glDispatchComputeIndirect(0);
	//_tspTraversal->deactivate();


	// Disk to PBO


	// To screen

	// rebind the previous FBO
	glBindFramebuffer(GL_FRAMEBUFFER, activeFBO);
}

void RenderableFlare::update(const UpdateData& data) {
}


} // namespace openspace
