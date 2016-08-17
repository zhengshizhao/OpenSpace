/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2016                                                               *
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

#include <modules/base/rendering/screenspaceframebuffer.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/camera.h>

#include <openspace/rendering/renderer.h>
#include <openspace/rendering/abufferrenderer.h>
#include <openspace/rendering/framebufferrenderer.h>
#include <openspace/engine/wrapper/windowwrapper.h>

namespace {
    const std::string KeySize = "Size";
}

namespace openspace {

ScreenSpaceFramebuffer::ScreenSpaceFramebuffer(const ghoul::Dictionary& dictionary) 
    : ScreenSpaceRenderable(dictionary)
    , _size("size", "Size", glm::vec2(0.f), glm::vec2(0.f), glm::vec2(2000.f))
    , _framebuffer(nullptr)
{
    _id = id();
    setName("ScreenSpaceFramebuffer" + std::to_string(_id));

    addProperty(_size);
    if (dictionary.hasKeyAndValue<glm::vec2>(KeySize)) {
        _size = dictionary.value<glm::vec2>(KeySize);
    }
    else {
        _size = OsEng.windowWrapper().currentWindowResolution();
    }
    _size.onChange([this]() { createTexture(); });

    _scale.setValue(1.0f);
}

bool ScreenSpaceFramebuffer::initialize() {
    bool success = ScreenSpaceRenderable::initialize();

    createFragmentbuffer();

    return success && isReady();
}

bool ScreenSpaceFramebuffer::deinitialize() {
    _framebuffer->detachAll();
    removeAllRenderFunctions();

    return ScreenSpaceRenderable::deinitialize();
}

void ScreenSpaceFramebuffer::render() {
    if (!_renderFunctions.empty()){
        glm::vec2 size = _size;

        glViewport(
            0.f, 0.f, size.x, size.y
        );

        GLint defaultFBO = _framebuffer->getActiveObject();
        _framebuffer->activate();
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_ALPHA_TEST);
        for (const std::function<void()>& renderFunction : _renderFunctions){
            renderFunction();
        }
        _framebuffer->deactivate();

        glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
        glm::vec2 resolution = OsEng.windowWrapper().currentWindowResolution();
        glViewport(0, 0, resolution.x, resolution.y);
        
        glm::mat4 rotation = rotationMatrix();
        glm::mat4 translation = translationMatrix();
        //glm::mat4 scale = scaleMatrix();
        float textureRatio = size.y / size.x;

        glm::mat4 scale = glm::scale(
            glm::mat4(1.f),
            glm::vec3(_scale, _scale * textureRatio, 1.f)
        );
        //scale = glm::scale(scale, glm::vec3((1.0/xratio), (1.0/yratio), 1.0f));
        glm::mat4 modelTransform = rotation*translation*scale;
        draw(modelTransform);
    }
}

bool ScreenSpaceFramebuffer::isReady() const {
    return _shader && _texture;
}

void ScreenSpaceFramebuffer::setSize(glm::vec2 size) {
    _size = std::move(size);
}

void ScreenSpaceFramebuffer::addRenderFunction(std::function<void()> renderFunction) {
    _renderFunctions.push_back(std::move(renderFunction));
}

void ScreenSpaceFramebuffer::removeAllRenderFunctions() {
    _renderFunctions.clear();
}

void ScreenSpaceFramebuffer::createFragmentbuffer() {
    _framebuffer = std::make_unique<ghoul::opengl::FramebufferObject>();
    _framebuffer->activate();
    createTexture();
    _framebuffer->attachTexture(_texture.get(), GL_COLOR_ATTACHMENT0);
    _framebuffer->deactivate();
}

void ScreenSpaceFramebuffer::createTexture() {
    glm::vec2 size = _size;
    _texture = std::make_unique<ghoul::opengl::Texture>(glm::uvec3(size.x, size.y, 1));
    _texture->uploadTexture();
    _texture->setFilter(ghoul::opengl::Texture::FilterMode::Linear);

    _framebuffer->activate();
    _framebuffer->attachTexture(_texture.get(), GL_COLOR_ATTACHMENT0);
    _framebuffer->deactivate();
}

int ScreenSpaceFramebuffer::id() {
    static int id = 0;
    return id++;
}
} //namespace openspace