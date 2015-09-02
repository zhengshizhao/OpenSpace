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

#include <modules/multiresplane/rendering/renderablemultiresplane.h>

#include <openspace/util/constants.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

#include <modules/multiresplane/rendering/quadtreelist.h>
//#include <modules/multiresplane/rendering/imageatlasmanager.h>
//#include <modules/multiresplane/rendering/imagebrickselector.h>

namespace {
    const std::string _loggerCat = "RenderableMultiresPlane";
    const std::string KeyDataSource = "Source";
    const std::string KeyTransferFunction = "TransferFunction";
}

namespace openspace {

RenderableMultiresPlane::RenderableMultiresPlane (const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _quadtreeList(nullptr)
    //, _atlasManager(nullptr)
    //, _brickSelector(nullptr)
    , _size("size", "Size", glm::vec2(1,1), glm::vec2(0.f), glm::vec2(1.f, 25.f))
    , _shader(nullptr)
    , _quad(0)
    , _vertexPositionBuffer(0)
{
    std::string name;
    bool success = dictionary.getValue(constants::scenegraphnode::keyName, name);
    assert(success);

    _filename = "";
    success = dictionary.getValue(KeyDataSource, _filename);
    if (!success) {
        LERROR("Node '" << name << "' did not contain a valid '" <<  KeyDataSource << "'");
        return;
    }
    _filename = absPath(_filename);
    if (_filename == "") {
        return;
    }

    _quadtreeList = new QuadtreeList(_filename);
    //_atlasManager = new ImageAtlasManager(_quadtreeList);
    //_brickSelector = new ImageBrickSelector(_quadtreeList);

    setBoundingSphere(PowerScaledScalar(5.0, 2.0));
}

RenderableMultiresPlane::~RenderableMultiresPlane() {
    if (_quadtreeList)
        delete _quadtreeList;
    //if (_atlasManager)
    //    delete _atlasManager;
    //if (_brickSelector)
    //    delete _brickSelector;
}

bool RenderableMultiresPlane::initialize() {
    glGenVertexArrays(1, &_quad); // generate array
    glGenBuffers(1, &_vertexPositionBuffer); // generate buffer
    createPlane();

    bool success = _quadtreeList && _quadtreeList->load();

    if (success) {
        _brickIndices.resize(_quadtreeList->nBricksPerDim() * _quadtreeList->nBricksPerDim(), 0);
        //success &= _brickSelector->initialize();
    }


    auto shaderCallback = [this](ghoul::opengl::ProgramObject* program) {
	_validShader = false;
    };

    if (_shader == nullptr) {
        _shader = ghoul::opengl::ProgramObject::Build("MultiResPlaneProgram",
            "${MODULES}/multiresplane/shaders/multiresPlane.vert",
            "${MODULES}/multiresplane/shaders/multiresPlane.frag");
        success &= !!_shader;
    }
    _shader->setProgramObjectCallback(shaderCallback);

    //success &= _atlasManager && _atlasManager->initialize();

    return success;
}

bool RenderableMultiresPlane::deinitialize() {
    glDeleteVertexArrays(1, &_quad);
    _quad = 0;

    glDeleteBuffers(1, &_vertexPositionBuffer);
    _vertexPositionBuffer = 0;

    if (_quadtreeList)
        delete _quadtreeList;
    _quadtreeList = nullptr;

    //if (_atlasManager)
    //    delete _atlasManager;
    //_atlasManager = nullptr;

    //if (_brickSelector)
    //    delete _brickSelector;
    //_brickSelector = nullptr;
}

bool RenderableMultiresPlane::isReady() const {
    return true;
}

void RenderableMultiresPlane::render(const RenderData& data) {
    if(!_validShader) {
        if (_shader) {
            _shader->rebuildFromFile();
        }
        _validShader = true;
    }

    // Select bricks
    const int nTimesteps = _quadtreeList->nTimesteps();
    const int currentTimestep = _timestep % nTimesteps;

    //_brickSelector->setBrickBudget(_brickBudget);
    //_brickSelector->selectBrics(currentTimestep, _brickIndices);
    //_atlasManager->updateAtlas(_brickIndices);

    // Activate shader program
    _shader->activate();
    glm::mat4 transform = glm::mat4(1.0);

    // Set uniforms
    _shader->setUniform("viewProjection", data.camera.viewProjectionMatrix());
    _shader->setUniform("modelTransform", transform);
    setPscUniforms(_shader, &data.camera, data.position);

    // Bind texture atlas
    ghoul::opengl::TextureUnit unit;
    unit.activate();
    //_atlasManager->textureAtlas()->bind();
    _shader->setUniform("atlas", unit);

    // TODO: Bind atlas map

    glBindVertexArray(_quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    _shader->deactivate();
}

void RenderableMultiresPlane::createPlane() {
    // ============================
    //      GEOMETRY (quad)
    // ============================
    const GLfloat size = _size.value()[0];
    const GLfloat w = _size.value()[1];
    const GLfloat vertex_data[] = { // square of two triangles (sigh)
        //    x      y     z     w     s     t
        -size, -size, 0.0f, w, 0, 1,
        size, size, 0.0f, w, 1, 0,
        -size, size, 0.0f, w, 0, 0,
        -size, -size, 0.0f, w, 0, 1,
        size, -size, 0.0f, w, 1, 1,
        size, size, 0.0f, w, 1, 0,
    };

    glBindVertexArray(_quad); // bind array
    glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, reinterpret_cast<void*>(sizeof(GLfloat) * 4));
}

} // namespace openspace
