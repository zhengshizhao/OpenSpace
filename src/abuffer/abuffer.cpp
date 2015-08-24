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

#include <openspace/abuffer/abuffer.h>
#include <openspace/abuffer/abuffervolume.h>
#include <openspace/engine/openspaceengine.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/misc/dictionary.h>

#include <sgct.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>

namespace {
    const std::string _loggerCat = "ABuffer";
}

namespace openspace {

ABuffer::ABuffer()
    : _validShader(false)
    , _resolveShader(nullptr)
{

    updateDimensions();
}

ABuffer::~ABuffer() {
    if(_resolveShader)
        delete _resolveShader;
}

bool ABuffer::initializeABuffer() {
    // ============================
    //             SHADERS
    // ============================
    auto shaderCallback = [this](ghoul::opengl::ProgramObject* program) {
        // Error for visibility in log
        invalidate();
    };

    generateShaderSubstitutions();
    _resolveShader = ghoul::opengl::ProgramObject::Build(
        "ABufferResolve",
        "${SHADERS}/ABuffer/abufferResolveVertex.glsl",
        "${SHADERS}/ABuffer/abufferResolveFragment.glsl",
        _shaderSubstitutions);
    if (!_resolveShader)
        return false;
    _resolveShader->setProgramObjectCallback(shaderCallback);

    // Remove explicit callback and use programobject isDirty instead ---abock

    // ============================
    //         GEOMETRY (quad)
    // ============================
    const GLfloat size = 1.0f;
    const GLfloat vertex_data[] = {
    //  x      y      s     t
        -size, -size, 0.0f, 1.0f,
        +size, +size, 0.0f, 1.0f, 
        -size, +size, 0.0f, 1.0f, 
        -size, -size, 0.0f, 1.0f, 
        +size, -size, 0.0f, 1.0f, 
        +size, +size, 0.0f, 1.0f,
    };

    GLuint vertexPositionBuffer;
    glGenVertexArrays(1, &_screenQuad); // generate array
    glBindVertexArray(_screenQuad); // bind array
    glGenBuffers(1, &vertexPositionBuffer); // generate buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    return true;
}

bool ABuffer::reinitialize() {
    // set the total resolution for all viewports
    updateDimensions();
    return reinitializeInternal();
}

void ABuffer::resetBindings() {

}

int ABuffer::getTextureUnit(ghoul::opengl::Texture* texture) {
    if (_textureUnits.find(texture) == _textureUnits.end()) {
       return -1;
    }
    return _textureUnits[texture];
}

int ABuffer::getSsboBinding(int ssboId) {
    if (_bufferBindings.find(ssboId) == _bufferBindings.end()) {
        return -1;
    }
    return _bufferBindings[ssboId];
}

void ABuffer::resolve(float blackoutFactor) {
    if(!isValid()) {
        generateShaderSubstitutions();
        if (_resolveShader) {
            _resolveShader->rebuildWithDictionary(_shaderSubstitutions);
        }
        _validShader = true;
    }
    
    if (!_resolveShader)
       return;
    
    _resolveShader->activate();
    _resolveShader->setUniform("blackoutFactor", blackoutFactor);
    
    int nUsedUnits = 0;
    int nUsedBindings = 0;

    // map from texture to texture unit
    _textureUnits.clear();
    _bufferBindings.clear();
    
    for (auto volumePair : _aBufferVolumes) {
        ABufferVolume* volume = volumePair.second;
        // Distribute texure units to the required textures.
        std::vector<ghoul::opengl::Texture*> textures = volume->getTextures();
        for (ghoul::opengl::Texture* t : textures) {
            if (_textureUnits.find(t) == _textureUnits.end()) {
                _textureUnits[t] = nUsedUnits++;
            }
        }

        std::vector<int> bufferIds = volume->getBuffers();
        for (int id : bufferIds) {
            if (_bufferBindings.find(id) == _bufferBindings.end()) {
                _bufferBindings[id] = nUsedBindings++;
            }
        }

        // Let the volume upload textures and update uniforms.
        volume->preResolve(_resolveShader);
    }
    
    for(auto tu : _textureUnits) {
        ghoul::opengl::Texture* texture = tu.first;
        int unit = tu.second;
        glActiveTexture(GL_TEXTURE0 + unit);
        texture->bind();
    }

    for(auto bb : _bufferBindings) {
        int bufferId = bb.first;
        int binding = bb.second;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, bufferId);
    }
    
    glBindVertexArray(_screenQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    _resolveShader->deactivate();
}

int ABuffer::addVolume(ABufferVolume* volume) {
    int volumeId = nextId++;
    _aBufferVolumes.insert(std::pair<int, ABufferVolume*>(volumeId - 1, volume));
    invalidate();
    return volumeId;
}

/*void ABuffer::removeVolume(int id) {
    _aBufferVolumes.erase(id);
    invalidate();
}*/

void ABuffer::generateShaderSubstitutions() {

    ghoul::Dictionary helpersDictionary;
    ghoul::Dictionary volumesDictionary;
    std::set<std::string> helperPaths;

    for (const std::pair<int, ABufferVolume*>& p: _aBufferVolumes) {
        ghoul::Dictionary volumeDictionary;

        ABufferVolume* v = p.second;
        std::string helperPath = v->getHelperPath();
        std::string headerPath = v->getHeaderPath();

        volumeDictionary.setValue("id", v->getId());
        volumeDictionary.setValue("bitmask", 1 << p.first);
        volumeDictionary.setValue("headerPath", headerPath);

        if (helperPaths.find(helperPath) == helperPaths.end()) {
            std::stringstream helperId;
            helperId << helperPaths.size();
            helpersDictionary.setValue(helperId.str(), helperPath);
            helperPaths.insert(helperPath);
        }

        std::stringstream volumeIndex;
        volumeIndex << p.first;
        volumesDictionary.setValue(volumeIndex.str(), volumeDictionary);
    }

    ghoul::Dictionary mainDictionary;
    mainDictionary.setValue("helperPaths", helpersDictionary);
    mainDictionary.setValue("volumes", volumesDictionary);

    int nVolumes = _aBufferVolumes.size();
    mainDictionary.setValue("nVolumes", nVolumes);

    _shaderSubstitutions = std::move(mainDictionary);
}

bool ABuffer::isValid() {
    return _validShader;
}

void ABuffer::invalidate() {
    _validShader = false;
}

void ABuffer::updateDimensions() {
    _width = sgct::Engine::instance()->getActiveWindowPtr()->getXFramebufferResolution();
    _height = sgct::Engine::instance()->getActiveWindowPtr()->getYFramebufferResolution();
    _totalPixels = _width * _height;
}


} // openspace
