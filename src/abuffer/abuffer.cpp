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

#include <sgct.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>

namespace {

    const std::string generatedSettingsPath      = "${SHADERS_GENERATED}/ABufferSettings.hglsl";
    const std::string generatedHelpersPath       = "${SHADERS_GENERATED}/ABufferHelpers.hglsl";
    const std::string generatedHeadersPath       = "${SHADERS_GENERATED}/ABufferHeaders.hglsl";

    const std::string generatedStepSizeCallsPath = "${SHADERS_GENERATED}/ABufferStepSizeCalls.hglsl";
    const std::string generatedSamplerCallsPath  = "${SHADERS_GENERATED}/ABufferSamplerCalls.hglsl";
    const std::string generatedStepSizeFunctionsPath  = "${SHADERS_GENERATED}/ABufferStepSizeFunctions.hglsl";
    const std::string generatedTransferFunctionVisualizerPath =
                        "${SHADERS_GENERATED}/ABufferTransferFunctionVisualizer.hglsl";
    const std::string generatedSamplersPath      = "${SHADERS_GENERATED}/ABufferSamplers.hglsl";

    const std::string _loggerCat = "ABuffer";

}

namespace openspace {

ABuffer::ABuffer()
    : _validShader(false)
    , _resolveShader(nullptr)
    , _volumeStepFactor(0.0f)
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
        _validShader = false;
    };

    generateShaderSource();
    _resolveShader = ghoul::opengl::ProgramObject::Build(
        "ABufferResolve",
        "${SHADERS}/ABuffer/abufferResolveVertex.glsl",
        "${SHADERS}/ABuffer/abufferResolveFragment.glsl");
    if (!_resolveShader)
        return false;
    _resolveShader->setProgramObjectCallback(shaderCallback);

    // Remove explicit callback and use programobject isDirty instead ---abock

    // ============================
    //         GEOMETRY (quad)
    // ============================
    const GLfloat size = 1.0f;
    const GLfloat vertex_data[] = {
        //	  x      y     s     t
        -size, -size, 0.0f, 1.0f,
        size,    size, 0.0f, 1.0f, 
        -size,  size, 0.0f, 1.0f, 
        -size, -size, 0.0f, 1.0f, 
        size, -size, 0.0f, 1.0f, 
        size,    size, 0.0f, 1.0f,
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
    if( ! _validShader) {
        generateShaderSource();
        updateShader();
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

void ABuffer::registerGlslHelpers(std::string helpers) {
    _glslHelpers.push_back(helpers);
}

int ABuffer::addVolume(ABufferVolume* volume) {
    int volumeId = nextId++;
    _aBufferVolumes.insert(std::pair<int, ABufferVolume*>(volumeId - 1, volume));
    std::map<std::string, std::string> map;
    _glslDictionary.insert(std::pair<ABufferVolume*, std::map<std::string, std::string> >(volume, map));
    return volumeId;
}

    /*    void ABuffer::removeVolume(ABufferVolume* volume) {
    _aBufferVolumes.erase(volume);
    _glslDictionary.erase(volume);
    }*/

std::string ABuffer::getGlslName(ABufferVolume* volume, const std::string& key) {
    auto dictionaryPtr = _glslDictionary.find(volume);
    if (dictionaryPtr == _glslDictionary.end()) {
        LERROR("Trying to translate glsl name, but volume is not added to abuffer.");
        return "ERROR";
    }
    std::map<std::string, std::string>& dictionary = dictionaryPtr->second;
    if (dictionary.find(key) == dictionary.end()) {
        std::string translation = generateGlslName(key);
        dictionary.insert(std::pair<std::string, std::string>(key, translation));
        return translation;
    }
    return dictionary.at(key);
}
    
std::string ABuffer::generateGlslName(const std::string& name) {
    std::stringstream ss;
    ss << "_gen_" << name << "_" << nextGlslNameId++;
    return  ss.str();
}

bool ABuffer::updateShader() {
    if (_resolveShader == nullptr)
        return false;
    bool s = _resolveShader->rebuildFromFile();
    return s;
}

void ABuffer::generateShaderSource() {
    LDEBUG("Generating shader includes");
    generateHelpers();
    generateHeaders();
    generateStepSizeCalls();
    generateSamplerCalls();
    generateStepSizeFunctions();
    generateSamplers();
}


void ABuffer::generateHelpers() {
    std::ofstream f(absPath(generatedHelpersPath));
    for (std::string& str : _glslHelpers) {
       f << str << std::endl;
    }
    f.close();
}

void ABuffer::generateHeaders() {
    
    std::ofstream f(absPath(generatedHeadersPath));
    f << "#define MAX_VOLUMES " << std::to_string(_aBufferVolumes.size()) << "\n";

    for (auto volumePair : _aBufferVolumes) {
        f << volumePair.second->getHeader();
    }

    for (auto volumePair : _aBufferVolumes) {
        int id = volumePair.second->getId();
        f << "vec4 sampleVolume_" << id << "(vec3 samplePos, vec3 dir, float occludingAlpha, inout float maxStepSize);" << std::endl;
        f << "float getStepSize_" << id << "(vec3 samplePos, vec3 dir);" << std::endl;
    }

    if (_aBufferVolumes.size() < 1) {
        f.close();
        return;
    }

    size_t maxLoop = 1000000;
    f << "#define LOOP_LIMIT " << maxLoop << "\n";
    
    f.close();

}
    
void ABuffer::generateStepSizeCalls() {
    std::ofstream f(absPath(generatedStepSizeCallsPath));
    
    for (auto volumePair : _aBufferVolumes) {
        int index = volumePair.first;
        int id = volumePair.second->getId();
        f << "#ifndef SKIP_VOLUME_" << id << "\n"
          << "if((currentVolumeBitmask & " << (1 << index) << ") == " << (1 << index) << ") {" << std::endl
          << "  maxStepSizeLocal = getStepSize_" << id << "(volume_position[" << index << "], volume_direction[" << index << "]);" << std::endl
          << "  maxStepSize"  << " = maxStepSizeLocal/volume_scale[" << index << "];" << std::endl
          << "  nextStepSize = min(nextStepSize, maxStepSize);" << std::endl
          << "}" << std::endl
          << "float previousJitterDistance_" << id << " = 0.0;" << std::endl
          << "#endif" << std::endl;
    }
    f.close();
}

void ABuffer::generateSamplerCalls() {
    std::ofstream f(absPath(generatedSamplerCallsPath));
    
    for (auto volumePair : _aBufferVolumes) {
        int index = volumePair.first;
        int id = volumePair.second->getId();
        f << "#ifndef SKIP_VOLUME_" << id << "\n"
          << "if((currentVolumeBitmask & " << (1 << index) << ") == " << (1 << index) << ") {" << std::endl
          << "  stepSizeLocal = stepSize*volume_scale[" << index << "];" << std::endl
          << "  float jitteredStepSizeLocal = stepSizeLocal*jitterFactor;" << std::endl
          << "  vec3 jitteredPosition = volume_position[" << index << "] + volume_direction[" << index << "]*jitteredStepSizeLocal;" << std::endl
          << "  volume_position[" << index << "] += volume_direction[" << index << "]*stepSizeLocal;" << std::endl
          << "  vec4 contribution = sampleVolume_" << id << "(jitteredPosition, volume_direction[" << index << "], final_color.a, maxStepSizeLocal);" << std::endl
          << "  float sampleDistance = 10*(jitteredStepSizeLocal + previousJitterDistance_" << id << ");" << std::endl
          << "  blendStep(final_color, contribution, sampleDistance);" << std::endl
          << "  previousJitterDistance_" << id << " = stepSizeLocal - jitteredStepSizeLocal;" << std::endl
          << "  maxStepSize = maxStepSizeLocal/volume_scale[" << index << "];" << std::endl
          << "  nextStepSize = min(nextStepSize, maxStepSize);" << std::endl
          << "}" << std::endl
            
          << "#endif\n";
    }
    
    f.close();
}

void ABuffer::generateSamplers() {
    std::ofstream f(absPath(generatedSamplersPath));

    for (auto volumePair : _aBufferVolumes) {
        std::stringstream functionName;
        ABufferVolume* volume = volumePair.second;
        functionName << "sampleVolume_" << volume->getId();
        f << volume->getSampler(functionName.str());
    }
    
    f.close();
}

void ABuffer::generateStepSizeFunctions() {
    std::ofstream f(absPath(generatedStepSizeFunctionsPath));
    for (auto volumePair : _aBufferVolumes) {
        std::stringstream functionName;
        ABufferVolume* volume = volumePair.second;
        functionName << "getStepSize_" << volume->getId();
        f << volume->getStepSizeFunction(functionName.str());
    }
    
    f.close();
}

void ABuffer::invalidateABuffer() {
    LDEBUG("Shader invalidated");
    _validShader = false;
}

void ABuffer::updateDimensions() {
    _width = sgct::Engine::instance()->getActiveWindowPtr()->getXFramebufferResolution();
    _height = sgct::Engine::instance()->getActiveWindowPtr()->getYFramebufferResolution();
    _totalPixels = _width * _height;
}


} // openspace
