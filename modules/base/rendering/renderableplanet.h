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

#ifndef __RENDERABLEPLANET_H__
#define __RENDERABLEPLANET_H__

// open space includes
#include <openspace/rendering/renderable.h>

#include <openspace/properties/stringproperty.h>
#include <openspace/util/updatestructures.h>

#include <ghoul/opengl/textureunit.h>

#include <vector>
#include <string>

// ghoul includes
namespace ghoul {
    namespace opengl {
        class ProgramObject;
        class Texture;
    }
}

namespace openspace {

namespace planetgeometry {
class PlanetGeometry;
}

class RenderablePlanet : public Renderable {
public:
    // Shadow structure
    typedef struct {
        std::pair<std::string, float> source;
        std::pair<std::string, float> caster;
    } ShadowConf;

    struct ShadowRenderingStruct {
        float xu, xp;
        float rs, rc;
        glm::vec3 sourceCasterVec;
        glm::vec3 casterPositionVec;
        bool isShadowing;
    };

    // See: Precomputed Atmospheric Scattering from Bruneton et al. 
    // for explanation of the following parameters.
    
    const unsigned int TRANSMITTANCE_TABLE_WIDTH  = 256;
    const unsigned int TRANSMITTANCE_TABLE_HEIGHT = 64;

    const unsigned int IRRADIANCE_TABLE_WIDTH  = 64;
    const unsigned int IRRADIANCE_TABLE_HEIGHT = 16;

    const unsigned int DELTA_E_TABLE_WIDTH  = 64;
    const unsigned int DELTA_E_TABLE_HEIGHT = 16;


    /*const unsigned int TRANSMITTANCE_TABLE_WIDTH = 512;
    const unsigned int TRANSMITTANCE_TABLE_HEIGHT = 128;

    const unsigned int IRRADIANCE_TABLE_WIDTH = 128;
    const unsigned int IRRADIANCE_TABLE_HEIGHT = 32;

    const unsigned int DELTA_E_TABLE_WIDTH = 128;
    const unsigned int DELTA_E_TABLE_HEIGHT = 32;*/

    const unsigned int R_SAMPLES = 32;
    const unsigned int MU_SAMPLES = 128;
    const unsigned int MU_S_SAMPLES = 32;
    const unsigned int NU_SAMPLES = 8;
    
    /*const unsigned int R_SAMPLES = 64;
    const unsigned int MU_SAMPLES = 256;
    const unsigned int MU_S_SAMPLES = 64;
    const unsigned int NU_SAMPLES = 16;*/

public:
    RenderablePlanet(const ghoul::Dictionary& dictionary);
    ~RenderablePlanet();

    bool initialize() override;
    bool deinitialize() override;
    bool isReady() const override;

    void render(const RenderData& data) override;
    void update(const UpdateData& data) override;

protected:
    void loadTexture();

private:
    void loadComputationPrograms();
    void unloadComputationPrograms();
    void createComputationTextures();
    void deleteComputationTextures();
    void deleteUnusedComputationTextures();
    void loadAtmosphereDataIntoShaderProgram(std::unique_ptr<ghoul::opengl::ProgramObject> & shaderProg);
    void executeCalculations(const GLuint vao, const GLenum drawBuffers[2], const GLsizei vertexSize);
    void preCalculateAtmosphereParam();
    void createAtmosphereFBO();
    void createRenderQuad(GLuint * vao, GLuint * vbo, const GLfloat size);
    void renderQuadForCalc(const GLuint vao, const GLsizei size);
    void step3DTexture(std::unique_ptr<ghoul::opengl::ProgramObject> & shaderProg, 
        const int layer, const bool doCalc = true);
    void saveTextureToPPMFile(const GLenum color_buffer_attachment, const std::string & fileName, 
                            const int width, const int height) const;

private:
    properties::StringProperty _colorTexturePath;
    properties::StringProperty _nightTexturePath;
    properties::StringProperty _heightMapTexturePath;
    properties::StringProperty _cloudsTexturePath;
    properties::StringProperty _reflectanceTexturePath;

    std::unique_ptr<ghoul::opengl::ProgramObject> _programObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _transmittanceProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _irradianceProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _irradianceSupTermsProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _inScatteringProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _inScatteringSupTermsProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _deltaEProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _deltaSProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _deltaSSupTermsProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _deltaJProgramObject;
    std::unique_ptr<ghoul::opengl::ProgramObject> _atmosphereProgramObject;
    ghoul::opengl::TextureUnit _dummyTextureUnit;
    ghoul::opengl::TextureUnit _dummy3DTextureUnit;
    ghoul::opengl::TextureUnit _transmittanceTableTextureUnit;
    ghoul::opengl::TextureUnit _irradianceTableTextureUnit;
    ghoul::opengl::TextureUnit _inScatteringTableTextureUnit;
    ghoul::opengl::TextureUnit _deltaETableTextureUnit;
    ghoul::opengl::TextureUnit _deltaSRayleighTableTextureUnit;
    ghoul::opengl::TextureUnit _deltaSMieTableTextureUnit;
    ghoul::opengl::TextureUnit _deltaJTableTextureUnit;
    ghoul::opengl::TextureUnit _atmosphereTextureUnit;
    std::unique_ptr<ghoul::opengl::Texture> _texture;
    std::unique_ptr<ghoul::opengl::Texture> _nightTexture;
    std::unique_ptr<ghoul::opengl::Texture> _reflectanceTexture;
    std::unique_ptr<ghoul::opengl::Texture> _heightMapTexture;
    std::unique_ptr<ghoul::opengl::Texture> _cloudsTexture;
    GLuint _transmittanceTableTexture;
    GLuint _irradianceTableTexture;
    GLuint _inScatteringTableTexture;
    GLuint _deltaETableTexture;
    GLuint _deltaSRayleighTableTexture;
    GLuint _deltaSMieTableTexture;
    GLuint _deltaJTableTexture;
    GLuint _dummyTexture;
    GLuint _dummy3DTexture;
    GLuint _atmosphereTexture;
    GLuint _atmosphereFBO;
    GLuint _atmosphereRenderVAO;
    GLuint _atmosphereRenderVBO;

    properties::FloatProperty _heightExaggeration;

    planetgeometry::PlanetGeometry* _geometry;
    properties::BoolProperty _performShading;
    properties::IntProperty _rotation;
    float _alpha;
    std::vector< ShadowConf > _shadowConfArray;
    float _planetRadius;

    glm::dmat3 _stateMatrix;
    std::string _frame;
    std::string _target;
    bool _hasNightTexture;
    bool _hasReflectanceTexture;
    bool _hasHeightTexture;
    bool _hasCloudsTexture;
    bool _shadowEnabled;
    double _time;

    // Atmosphere Data
    bool _atmosphereCalculated;
    bool _atmosphereEnabled;
    float _atmosphereRadius;
    float _atmospherePlanetRadius;
    float _planetAverageGroundReflectance;
    float _rayleighHeightScale;
    float _mieHeightScale;
    float _miePhaseConstant;
    glm::vec3 _mieExtinctionCoeff;
    glm::vec3 _rayleighScatteringCoeff;
    glm::vec3 _mieScatteringCoeff;


    bool tempPic;

    unsigned int count;
};

}  // namespace openspace

#endif  // __RENDERABLEPLANET_H__