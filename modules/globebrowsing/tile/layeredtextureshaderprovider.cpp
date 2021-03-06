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

#include <modules/globebrowsing/tile/layeredtextureshaderprovider.h>

#include <openspace/rendering/renderengine.h>
#include <openspace/engine/openspaceengine.h>

#include "ghoul/misc/dictionary.h"

#include <string>

namespace {
    const std::string _loggerCat = "LayeredTextureShaderProvider";
}

namespace openspace {

    const std::string LayeredTextureInfo::glslKeyPrefixes[NUM_SETTINGS_PER_CATEGORY] =
    {
        "lastLayerIndex",
        "use",
        "blend",
    };

    bool LayeredTextureInfo::operator==(const LayeredTextureInfo& other) const
    {
        return
            lastLayerIdx == other.lastLayerIdx &&
            layerBlendingEnabled == other.layerBlendingEnabled;
    }

    bool LayeredTexturePreprocessingData::operator==(
        const LayeredTexturePreprocessingData& other) const
    {
        
        if (layeredTextureInfo.size() != other.layeredTextureInfo.size() ||
            keyValuePairs.size() != other.keyValuePairs.size()) {
            return false;
        }
        else
        {
            bool equal = true;
            for (size_t i = 0; i < layeredTextureInfo.size(); i++) {
                equal = equal && (layeredTextureInfo[i] == other.layeredTextureInfo[i]);
            }
            for (size_t i = 0; i < keyValuePairs.size(); i++) {
                equal = equal && (keyValuePairs[i] == other.keyValuePairs[i]);
            }
            return equal;
        }
    }

    LayeredTextureShaderProvider::LayeredTextureShaderProvider(
        const std::string& shaderName,
        const std::string& vsPath,
        const std::string& fsPath)
        : _shaderName(shaderName)
        , _vsPath(vsPath)
        , _fsPath(fsPath)
        , _updatedOnLastCall(false)
    {
    
    }
    
    LayeredTextureShaderProvider::~LayeredTextureShaderProvider()
    {
        if (_programObject) {
            RenderEngine& renderEngine = OsEng.renderEngine();
            renderEngine.removeRenderProgram(_programObject);
            _programObject = nullptr;
        }
    }

    ProgramObject* LayeredTextureShaderProvider::getUpdatedShaderProgram(
        LayeredTexturePreprocessingData preprocessingData)
    {
        _updatedOnLastCall = false;
        if (!(preprocessingData == _preprocessingData) || _programObject == nullptr)
        { // No need to recompile shader. Shader is up to date and can be returned.
            recompileShaderProgram(preprocessingData);
            _updatedOnLastCall = true;
        }
        return _programObject.get();
    }

    void LayeredTextureShaderProvider::recompileShaderProgram(
        LayeredTexturePreprocessingData preprocessingData)
    {
        _preprocessingData = preprocessingData;
        ghoul::Dictionary shaderDictionary;
        

        // Different texture types can be height maps or color texture for example
        // These are used differently within the shaders.
        auto textureTypes = _preprocessingData.layeredTextureInfo;
        for (size_t i = 0; i < textureTypes.size(); i++) {
            // lastLayerIndex must be at least 0 for the shader to compile,
            // the layer type is inactivated by setting useThisLayerType to false
            shaderDictionary.setValue(
                LayeredTextureInfo::glslKeyPrefixes[LayeredTextureInfo::GlslKeyPrefixes::lastLayerIndex] +
                LayeredTextures::TEXTURE_CATEGORY_NAMES[i], glm::max(textureTypes[i].lastLayerIdx, 0));
            shaderDictionary.setValue(
                LayeredTextureInfo::glslKeyPrefixes[LayeredTextureInfo::GlslKeyPrefixes::use] +
                LayeredTextures::TEXTURE_CATEGORY_NAMES[i], textureTypes[i].lastLayerIdx >= 0);
            shaderDictionary.setValue(
                LayeredTextureInfo::glslKeyPrefixes[LayeredTextureInfo::GlslKeyPrefixes::blend] +
                LayeredTextures::TEXTURE_CATEGORY_NAMES[i], textureTypes[i].layerBlendingEnabled);
        }

        // Other settings such as "useAtmosphere"
        auto keyValuePairs = _preprocessingData.keyValuePairs;
        for (size_t i = 0; i < keyValuePairs.size(); i++) {
            shaderDictionary.setValue(keyValuePairs[i].first, keyValuePairs[i].second);
        }
        // Remove old program
        _programObject.release();

        _programObject = OsEng.renderEngine().buildRenderProgram(
            _shaderName,
            _vsPath,
            _fsPath,
            shaderDictionary);

        ghoul_assert(_programObject != nullptr, "Failed to initialize programObject!");
        using IgnoreError = ProgramObject::IgnoreError;
        _programObject->setIgnoreSubroutineUniformLocationError(IgnoreError::Yes);
    }

    bool LayeredTextureShaderProvider::updatedOnLastCall() {
        return _updatedOnLastCall;
    }


    const std::string LayeredTextureShaderUniformIdHandler::glslTileDataNames[NUM_TILE_DATA_VARIABLES] =
    {
        "textureSampler",
        "depthTransform.depthScale",
        "depthTransform.depthOffset",
        "uvTransform.uvOffset",
        "uvTransform.uvScale"
    };

    const std::string LayeredTextureShaderUniformIdHandler::blendLayerSuffixes[NUM_BLEND_TEXTURES] =
    {
        "",
        "Parent1",
        "Parent2",
    };

    LayeredTextureShaderUniformIdHandler::LayeredTextureShaderUniformIdHandler()
    {

    }

    LayeredTextureShaderUniformIdHandler::~LayeredTextureShaderUniformIdHandler()
    {
    }

    void LayeredTextureShaderUniformIdHandler::updateIdsIfNecessary(LayeredTextureShaderProvider* shaderProvider)
    {
        if (shaderProvider->updatedOnLastCall())
        {
            _shaderProvider = shaderProvider;
            _shaderProvider->_programObject->setIgnoreUniformLocationError(ProgramObject::IgnoreError::Yes);
            for (size_t i = 0; i < LayeredTextures::NUM_TEXTURE_CATEGORIES; i++)
            {
                for (size_t j = 0; j < NUM_BLEND_TEXTURES; j++)
                {
                    for (size_t k = 0; k < LayeredTextures::MAX_NUM_TEXTURES_PER_CATEGORY; k++)
                    {
                        for (size_t l = 0; l < NUM_TILE_DATA_VARIABLES; l++)
                        {
                            _tileUniformIds[i][j][k][l] = _shaderProvider->_programObject->uniformLocation(
                                LayeredTextures::TEXTURE_CATEGORY_NAMES[i] +
                                blendLayerSuffixes[j] +
                                "[" + std::to_string(k) + "]." +
                                glslTileDataNames[l]);
                        }
                    }
                }
            }
            _shaderProvider->_programObject->setIgnoreUniformLocationError(ProgramObject::IgnoreError::No);
        }
    }

    GLint LayeredTextureShaderUniformIdHandler::getId(
        LayeredTextures::TextureCategory category,
        size_t blendLayer,
        size_t layerIndex,
        GlslTileDataId tileDataId)
    {
        return _tileUniformIds[category][blendLayer][layerIndex][tileDataId];
    }

    
    ProgramObject& LayeredTextureShaderUniformIdHandler::programObject()
    {
        return *_shaderProvider->_programObject;
    }
    
}  // namespace openspace
