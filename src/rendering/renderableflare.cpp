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
#include <openspace/engine/openspaceengine.h>
#include <openspace/abuffer/abuffer.h>
#include <openspace/rendering/flare/tsp.h>
#include <openspace/rendering/flare/brickmanager.h>

// ghoul includes
#include <ghoul/opengl/framebufferobject.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/io/texture/texturereader.h>
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
	: RenderableVolume(dictionary)
	, _tsp(nullptr)
	, _brickManager(nullptr)
	, _boxArray(0)
	, _dispatchBuffers()
	, _tspTraversal(nullptr)
	, _raycasterTsp(nullptr)
	, _cubeProgram(nullptr)
	, _textureToAbuffer(nullptr)
	, _fbo(nullptr)
	, _backTexture(nullptr)
	, _outputTexture(nullptr)
	, _transferFunction(nullptr)
	, _timestep(3)
{
	std::string s;
	dictionary.getValue(keyDataSource, s);
	s = absPath(s);
	if (!FileSys.fileExists(s, true))
		return;

	_tsp = new TSP(s);
	_brickManager = new BrickManager(_tsp);

	std::string transferfunctionPath;
	if (dictionary.getValue(keyTransferFunction, transferfunctionPath)) {
		transferfunctionPath = findPath(transferfunctionPath);
		if (transferfunctionPath != "")
			_transferFunction = loadTransferFunction(transferfunctionPath);
		if (!_transferFunction)
			LERROR("Could not load transferfunction");
	}

	dictionary.getValue(keyTSPTraversal, _traversalPath);
	dictionary.getValue(keyRaycasterTSP, _raycasterPath);
	_traversalPath = findPath(_traversalPath);
	_raycasterPath = findPath(_raycasterPath);
	
	setBoundingSphere(PowerScaledScalar(2.0, 0.0));
}

RenderableFlare::~RenderableFlare() {
	if (_tsp)
		delete _tsp;
	if (_brickManager)
		delete _brickManager;
	if (_dispatchBuffers[0])
		glDeleteBuffers(2, _dispatchBuffers);
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
	if (_transferFunction)
		delete _transferFunction;
}

bool RenderableFlare::initialize() {
	bool success = true;

	if (_tsp) {
		success &= _tsp->load();
	}
	if (_brickManager) {
		success &= _brickManager->readHeader();
		success &= _brickManager->initialize();
	}
	
	if (success) {

		OsEng.configurationManager().getValue("pscColorToTexture", _cubeProgram);
		OsEng.configurationManager().getValue("pscTextureToABuffer", _textureToAbuffer);

		/*
		static const struct
		{
			GLuint num_groups_x;
			GLuint num_groups_y;
			GLuint num_groups_z;
		} dispatch_params = { 1280 / 16, 720 / 16, 1 };
		glGenBuffers(2, _dispatchBuffers);
		*/
		_tspTraversal = ghoul::opengl::ProgramObject::Build(
			"tsptraversal", 
			findPath("passthrough_vs.glsl"), 
			_traversalPath);
		if (!_tspTraversal)
			LERROR("Could not build _tspTraversal");

		_tspTraversal->setIgnoreUniformLocationError(true);


		_raycasterTsp = ghoul::opengl::ProgramObject::Build(
			"raycasterTsp",
			findPath("passthrough_vs.glsl"),
			_raycasterPath);
		if (!_raycasterTsp)
			LERROR("Could not build _raycasterTsp");

		initializeColorCubes();

		if (_transferFunction)
			_transferFunction->uploadTexture();

		// Allocate space for the brick request list
		// Use 0 as default value
		_brickRequest.resize(_tsp->numTotalNodes(), 0);
		/*
		glGenBuffers(1, &_brickRequestBuffer); // generate buffer
		glBindBuffer(GL_TEXTURE_BUFFER, _brickRequestBuffer);
		glBufferData(GL_TEXTURE_BUFFER, sizeof(GLint)*_tsp->numTotalNodes(), NULL, GL_DYNAMIC_READ);

		glGenTextures(1, &_brickRequestTexture);
		glBindTexture(GL_TEXTURE_BUFFER, _brickRequestTexture);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, _brickRequestBuffer);
		glBindTexture(GL_TEXTURE_BUFFER, 0);

		GLuint* data = (GLuint*)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
		memset(data, 0x00, _tsp->numTotalNodes() * sizeof(GLint));
		glUnmapBuffer(GL_TEXTURE_BUFFER);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);

		*/
		glGenBuffers(1, &_reqeustedBrickSSO);
		glGenBuffers(1, &_brickSSO);
		_brickSSOSize = 0;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _reqeustedBrickSSO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLint)*_tsp->numTotalNodes(), NULL, GL_DYNAMIC_READ);
		GLint* data = (GLint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
		memset(data, 0x00, _tsp->numTotalNodes() * sizeof(GLint));
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Prepare the first timestep
		// TODO: How does this work without correct color cube?
		//launchTSPTraversal(0);
		//readRequestedBricks();
		//_brickManager->BuildBrickList(BrickManager::EVEN, _brickRequest);
		//_brickManager->DiskToPBO(BrickManager::EVEN);
		
	}

	return success;
}

bool RenderableFlare::deinitialize() {
	return true;
}

bool RenderableFlare::isReady() const {
	return true;
}

void RenderableFlare::render(const RenderData& data) {
	//glEnable(GL_SCISSOR_TEST);
	//glScissor(1280 / 2, 720 / 2, 1, 1);

	const unsigned int currentTimestep = _timestep++ % _tsp->header().numTimesteps_;
	const unsigned int nextTimestep = currentTimestep < _tsp->header().numTimesteps_ - 1 ? currentTimestep + 1 : 0;

	BrickManager::BUFFER_INDEX currentBuf, nextBuf;
	if (currentTimestep % 2 == 0) {
		currentBuf = BrickManager::EVEN;
		nextBuf = BrickManager::ODD;
	}
	else {
		currentBuf = BrickManager::ODD;
		nextBuf = BrickManager::EVEN;
	}

	// Render color cubes
	renderColorCubeTextures(data);

	// Dispatch TSP traversal
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	_tspTraversal->activate();

	// Bind textures
	ghoul::opengl::TextureUnit unit1;
	unit1.activate();
	_backTexture->bind();

	// Set uniforms
	int timesteps = static_cast<int>(_tsp->header().numTimesteps_);
	int numOTNodes = static_cast<int>(_tsp->numOTNodes());

	_tspTraversal->setUniform("cubeBack", unit1);
	_tspTraversal->setUniform("gridType", _tsp->header().gridType_);
	_tspTraversal->setUniform("stepSize", 0.02f);
	_tspTraversal->setUniform("numTimesteps", timesteps);
	_tspTraversal->setUniform("numValuesPerNode", _tsp->numValuesPerNode());
	_tspTraversal->setUniform("numOTNodes", numOTNodes);
	_tspTraversal->setUniform("temporalTolerance", -1.0f);
	_tspTraversal->setUniform("spatialTolerance", -1.0f);
	_tspTraversal->setUniform("timestep", nextTimestep);
	_tspTraversal->setUniform("modelViewProjection", data.camera.viewProjectionMatrix());
	_tspTraversal->setUniform("modelTransform", glm::mat4(1.0));
	setPscUniforms(_tspTraversal, &data.camera, data.position);

	/*
	GLint d;
	for (int i = 0; i < 8; ++i) {
	d = 0;
	glGetIntegeri_v(GL_IMAGE_BINDING_NAME, i, &d);
	LDEBUG("d" << i << ": " << d);
	}
	*/

	// bind textures
	GLint i = _tspTraversal->uniformLocation("out_image");
	glBindVertexArray(_boxArray);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _tsp->ssbo());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _reqeustedBrickSSO);
	glBindImageTexture(3, *_outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// Dispatch
	//glBindImageTexture(3, *_outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glDrawArrays(GL_TRIANGLES, 0, 6 * 6);

	_tspTraversal->deactivate();
	//glBindImageTexture(3, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	// PBO to atlas
	_brickManager->PBOToAtlas(currentBuf);

	// Read _brickSSO
	readRequestedBricks();

	// Dispatch Raycaster for currentTimestep
	//launchRaycaster(currentTimestep, _brickManager->brickList(currentBuf));

	// Disk to PBO
	_brickManager->BuildBrickList(nextBuf, _brickRequest);
	_brickManager->DiskToPBO(nextBuf);

	// To screen
	OsEng.renderEngine().abuffer()->resetBindings();
	_textureToAbuffer->activate();

	setPscUniforms(_textureToAbuffer, &data.camera, data.position);
	_textureToAbuffer->setUniform("modelViewProjection", data.camera.viewProjectionMatrix());
	_textureToAbuffer->setUniform("modelTransform", glm::mat4(1.0));

	// Bind texture
	//ghoul::opengl::TextureUnit unit;
	//unit.activate();
	//_outputTexture->bind();
	//_textureToAbuffer->setUniform("texture1", unit);
	glBindImageTexture(3, *_outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	glBindVertexArray(_boxArray);
	//glBindImageTexture(3, *_outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glDrawArrays(GL_TRIANGLES, 0, 6 * 6);
	_textureToAbuffer->deactivate();
	glDisable(GL_CULL_FACE);

	ghoul::opengl::ProgramObject* pscColorPassthrough = nullptr;
	if (OsEng.configurationManager().getValue("pscColorPassthrough", pscColorPassthrough)) {
		pscColorPassthrough->activate();


		setPscUniforms(pscColorPassthrough, &data.camera, data.position);
		pscColorPassthrough->setUniform("modelViewProjection", data.camera.viewProjectionMatrix());
		pscColorPassthrough->setUniform("modelTransform", glm::mat4(1.0));

		glBindVertexArray(_boxArray);
		glDrawArrays(GL_LINES, 0, 6 * 6);
		pscColorPassthrough->deactivate();
	}

	//glDisable(GL_SCISSOR_TEST);
}

void RenderableFlare::update(const UpdateData& data) {
}

//////////////////////////////////////////////////////////////////////////////////////////
// Flare internal functions
//////////////////////////////////////////////////////////////////////////////////////////
void RenderableFlare::launchTSPTraversal(int timestep){

	
	//glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void RenderableFlare::readRequestedBricks() {
	memset(_brickRequest.data(), 0, _brickRequest.size()*sizeof(int));
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Might not work on AMD 
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _reqeustedBrickSSO);
#if 0
	GLint* d = (GLint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	memcpy(_brickRequest.data(), d, sizeof(GLint)*_tsp->numTotalNodes());
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
#else
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLint) *_tsp->numTotalNodes(), _brickRequest.data());
#endif
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	for (auto x : _brickRequest) {
		if (x != 0)
			LDEBUG(x);
	}
}

void RenderableFlare::launchRaycaster(int timestep, const std::vector<int>& brickList) {
	_raycasterTsp->activate();

	// Bind textures
	ghoul::opengl::TextureUnit unit1;
	ghoul::opengl::TextureUnit unit2;
	ghoul::opengl::TextureUnit unit3;
	unit1.activate();
	_backTexture->bind();
	unit2.activate();
	_transferFunction->bind();
	unit3.activate();
	_brickManager->textureAtlas()->bind();

	// Set uniforms
	int timesteps = static_cast<int>(_tsp->header().numTimesteps_);
	int numOTNodes = static_cast<int>(_tsp->numOTNodes());
	int rootLevel = static_cast<int>(_tsp->numOTLevels()) - 1;
	int paddedBrickDim = static_cast<int>(_tsp->paddedBrickDim());
	int numBricksPerAxis = static_cast<int>(_tsp->numBricksPerAxis());

	_raycasterTsp->setUniform("cubeBack", unit1);
	_raycasterTsp->setUniform("transferFunction", unit2);
	_raycasterTsp->setUniform("textureAtlas", unit3);
	
	_raycasterTsp->setUniform("gridType", _tsp->header().gridType_);
	_raycasterTsp->setUniform("stepSize", 0.02f);
	_raycasterTsp->setUniform("numTimesteps", timesteps);
	_raycasterTsp->setUniform("numValuesPerNode", _tsp->numValuesPerNode());
	_raycasterTsp->setUniform("numOTNodes", numOTNodes);
	_raycasterTsp->setUniform("temporalTolerance", -1.0f);
	_raycasterTsp->setUniform("spatialTolerance", -1.0f);
	_raycasterTsp->setUniform("timestep", timestep);
	_raycasterTsp->setUniform("rootLevel", rootLevel);
	_raycasterTsp->setUniform("paddedBrickDim", paddedBrickDim);
	_raycasterTsp->setUniform("numBoxesPerAxis", numBricksPerAxis);

	// set bricks data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _brickSSO);
	GLuint size = sizeof(GLint)*brickList.size();
	if (size < _brickSSOSize) {
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, brickList.data(), GL_DYNAMIC_READ);
		_brickSSOSize = size;
	}
	else {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, brickList.data());
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	// bind textures
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _tsp->ssbo());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _brickSSO);
	glBindImageTexture(3, *_outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	//glBindImageTexture(4, *_brickManager->textureAtlas(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

	// Dispatch
	glDispatchComputeIndirect(0);

	_raycasterTsp->deactivate();
}

void RenderableFlare::PBOToAtlas(size_t buffer){
}

void RenderableFlare::buildBrickList(size_t buffer, const Bricks& bricks){
}

void RenderableFlare::diskToPBO(size_t buffer){
}


//////////////////////////////////////////////////////////////////////////////////////////
// Internal helper functions
//////////////////////////////////////////////////////////////////////////////////////////
void RenderableFlare::initializeColorCubes() {
	// ============================
	//      GEOMETRY (box)
	// ============================
	const GLfloat size = 0.5f;
	const GLfloat _w = 0.0f;
	const GLfloat vertex_data[] = {
		//  x,     y,     z,     s,
		-size, -size, size, _w, 0.0, 0.0, 1.0, 1.0,
		size, -size, size, _w, 1.0, 0.0, 1.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,
		-size, size, size, _w, 0.0, 1.0, 1.0, 1.0,
		-size, -size, size, _w, 0.0, 0.0, 1.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,

		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		-size, size, -size, _w, 0.0, 1.0, 0.0, 1.0,
		size, size, -size, _w, 1.0, 1.0, 0.0, 1.0,
		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		size, size, -size, _w, 1.0, 1.0, 0.0, 1.0,
		size, -size, -size, _w, 1.0, 0.0, 0.0, 1.0,

		size, -size, -size, _w, 1.0, 0.0, 0.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,
		size, -size, size, _w, 1.0, 0.0, 1.0, 1.0,
		size, -size, -size, _w, 1.0, 0.0, 0.0, 1.0,
		size, size, -size, _w, 1.0, 1.0, 0.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,

		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		-size, size, size, _w, 0.0, 1.0, 1.0, 1.0,
		-size, size, -size, _w, 0.0, 1.0, 0.0, 1.0,
		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		-size, -size, size, _w, 0.0, 0.0, 1.0, 1.0,
		-size, size, size, _w, 0.0, 1.0, 1.0, 1.0,

		-size, size, -size, _w, 0.0, 1.0, 0.0, 1.0,
		-size, size, size, _w, 0.0, 1.0, 1.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,
		-size, size, -size, _w, 0.0, 1.0, 0.0, 1.0,
		size, size, size, _w, 1.0, 1.0, 1.0, 1.0,
		size, size, -size, _w, 1.0, 1.0, 0.0, 1.0,

		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		size, -size, size, _w, 1.0, 0.0, 1.0, 1.0,
		-size, -size, size, _w, 0.0, 0.0, 1.0, 1.0,
		-size, -size, -size, _w, 0.0, 0.0, 0.0, 1.0,
		size, -size, -size, _w, 1.0, 0.0, 0.0, 1.0,
		size, -size, size, _w, 1.0, 0.0, 1.0, 1.0,
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
	_fbo->activate();
	int x1, xSize, y1, ySize;
	sgct::Engine::instance()->getActiveWindowPtr()->getCurrentViewportPixelCoords(x1, y1, xSize, ySize);
	size_t x = static_cast<size_t>(xSize);
	size_t y = static_cast<size_t>(ySize);

	const ghoul::opengl::Texture::Format format = ghoul::opengl::Texture::Format::RGBA;
	GLint internalFormat = GL_RGBA;
	GLenum dataType = GL_FLOAT;

	_backTexture = new Texture(glm::size3_t(x, y, 1));
	_outputTexture = new Texture(glm::size3_t(x, y, 1), format, GL_RGBA32F, dataType);
	_backTexture->uploadTexture();
	_outputTexture->uploadTexture();
	_fbo->attachTexture(_backTexture, GL_COLOR_ATTACHMENT0);
	_fbo->deactivate();
}

void RenderableFlare::renderColorCubeTextures(const RenderData& data) {
	GLuint activeFBO = FramebufferObject::getActiveObject(); // Save SGCTs main FBO
	_fbo->activate();
	_cubeProgram->activate();

	const Camera& camera = data.camera;
	const psc& position = data.position;
	setPscUniforms(_cubeProgram, &camera, position);
	_cubeProgram->setUniform("modelViewProjection", camera.viewProjectionMatrix());
	_cubeProgram->setUniform("modelTransform", glm::mat4(1.0));

	sgct_core::Frustum::FrustumMode mode = sgct::Engine::instance()->
		getActiveWindowPtr()->
		getCurrentViewport()->
		getEye();

	// If stereo is activated we don't want to clear the 
	if (mode == sgct_core::Frustum::FrustumMode::Mono ||
		mode == sgct_core::Frustum::FrustumMode::StereoLeftEye) {
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

	}
	// make sure GL_CULL_FACE is enabled (it should be disabled for the abuffer)
	glEnable(GL_CULL_FACE);

	//      Draw backface
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glCullFace(GL_FRONT);
	glBindVertexArray(_boxArray);
	glDrawArrays(GL_TRIANGLES, 0, 6 * 6);
	_fbo->deactivate();
	glCullFace(GL_BACK);

	// rebind the previous FBO
	glBindFramebuffer(GL_FRAMEBUFFER, activeFBO);

}

} // namespace openspace
