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

#include <openspace/engine/configurationmanager.h>
#include <modules/newhorizons/rendering/renderableshadowcylinder.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/powerscaledcoordinate.h>
#include <openspace/util/spicemanager.h>

#include <ghoul/filesystem/filesystem>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>


namespace {
    const std::string _loggerCat      = "RenderablePlane";
    const std::string _keyType        = "TerminatorType";
    const std::string _keyLightSource = "LightSource";
    const std::string _keyObserver    = "Observer";
    const std::string _keyBody        = "Body";
    const std::string _keyBodyFrame   = "BodyFrame";
    const std::string _keyMainFrame   = "MainFrame";
    const std::string _keyAberration  = "Aberration";
}

namespace openspace {
    RenderableShadowCylinder::RenderableShadowCylinder(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _numberOfPoints("amountOfPoints", "Points", 190, 1, 300)
    , _shadowLength("shadowLength", "Shadow Length", 0.1, 0.0, 0.5)
    , _shadowColor("shadowColor", "Shadow Color",
                   glm::vec4(1.f, 1.f, 1.f, 0.25f), glm::vec4(0.f), glm::vec4(1.f))
    , _shader(nullptr)
    , _vao(0)
    , _vbo(0)
{
    addProperty(_numberOfPoints);
    addProperty(_shadowLength);
    addProperty(_shadowColor);

    bool success = dictionary.getValue(_keyType, _terminatorType);
    ghoul_assert(success, "");
    success = dictionary.getValue(_keyLightSource, _lightSource);
    ghoul_assert(success, "");
    success = dictionary.getValue(_keyObserver, _observer);
    ghoul_assert(success, "");
    success = dictionary.getValue(_keyBody, _body);
    ghoul_assert(success, "");
    success = dictionary.getValue(_keyBodyFrame, _bodyFrame);
    ghoul_assert(success, "");
    success = dictionary.getValue(_keyMainFrame, _mainFrame);
    ghoul_assert(success, "");
    std::string a = "NONE";
    success = dictionary.getValue(_keyAberration, a);
    _aberration = SpiceManager::AberrationCorrection(a);
    ghoul_assert(success, "");
}

RenderableShadowCylinder::~RenderableShadowCylinder() {}

bool RenderableShadowCylinder::isReady() const {
    bool ready = true;
    if (!_shader)
        ready &= false;
    return ready;
}

bool RenderableShadowCylinder::initialize() {
    glGenVertexArrays(1, &_vao); // generate array
    glGenBuffers(1, &_vbo); // generate buffer

    bool completeSuccess = true;

    RenderEngine& renderEngine = OsEng.renderEngine();
    _shader = renderEngine.buildRenderProgram("ShadowProgram",
        "${MODULE_NEWHORIZONS}/shaders/terminatorshadow_vs.glsl",
        "${MODULE_NEWHORIZONS}/shaders/terminatorshadow_fs.glsl");

    if (!_shader)
        return false;


    return completeSuccess;
}

bool RenderableShadowCylinder::deinitialize() {
    RenderEngine& renderEngine = OsEng.renderEngine();
    if (_shader) {
        renderEngine.removeRenderProgram(_shader);
        _shader = nullptr;
    }

    glDeleteVertexArrays(1, &_vao);
    _vao = 0;
    glDeleteBuffers(1, &_vbo);
    _vbo = 0;

    return true;
}

void RenderableShadowCylinder::render(const RenderData& data){
    glm::mat4 _transform = glm::mat4(1.0);
    for (int i = 0; i < 3; i++){
        for (int j = 0; j < 3; j++){
            _transform[i][j] = static_cast<float>(_stateMatrix[i][j]);
        }
    }

    glDepthMask(false);
    // Activate shader
    _shader->activate();

    _shader->setUniform("ViewProjection", data.camera.viewProjectionMatrix());
    _shader->setUniform("ModelTransform", _transform);
    _shader->setUniform("shadowColor", _shadowColor);
    setPscUniforms(*_shader.get(), data.camera, data.position);
    
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(_vertices.size()));
    glBindVertexArray(0);

    _shader->deactivate();

    glDepthMask(true);
}

void RenderableShadowCylinder::update(const UpdateData& data) {
    _stateMatrix = SpiceManager::ref().positionTransformMatrix(_bodyFrame, _mainFrame, data.time);
    _time = data.time;
    if (_shader->isDirty())
        _shader->rebuildFromFile();
    createCylinder();
}

glm::vec4 psc_addition(glm::vec4 v1, glm::vec4 v2) {
    float k = 10.f;
    float ds = v2.w - v1.w;
    if (ds >= 0) {
        float p = pow(k, -ds);
        return glm::vec4(v1.x*p + v2.x, v1.y*p + v2.y, v1.z*p + v2.z, v2.w);
    }
    else {
        float p = pow(k, ds);
        return glm::vec4(v1.x + v2.x*p, v1.y + v2.y*p, v1.z + v2.z*p, v1.w);
    }
}

void RenderableShadowCylinder::createCylinder() {
    double targetEpoch;
    glm::dvec3 observerPosition;
    std::vector<psc> terminatorPoints;
    SpiceManager::TerminatorType t = SpiceManager::terminatorTypeFromString(_terminatorType);
//    if (_terminatorType == "UMBRAL")
//        t = SpiceManager::TerminatorType::Umbral;
//    else if (_terminatorType == "PENUMBRAL")
//        t = SpiceManager::TerminatorType::Penumbral;
    
    auto res = SpiceManager::ref().terminatorEllipse(_body, _observer, _bodyFrame,
        _lightSource, t, _aberration, _time, _numberOfPoints);
    
    targetEpoch = res.targetEphemerisTime;
    observerPosition = std::move(res.observerPosition);
    
    std::vector<glm::dvec3> ps = std::move(res.terminatorPoints);
    for (auto&& p : ps) {
        PowerScaledCoordinate psc = PowerScaledCoordinate::CreatePowerScaledCoordinate(p.x, p.y, p.z);
        psc[3] += 3;
        terminatorPoints.push_back(psc);
    }
    
    double lt;
    glm::dvec3 vecLightSource =
        SpiceManager::ref().targetPosition(_body, _lightSource, _mainFrame, _aberration, _time, lt);

    glm::dmat3 _stateMatrix = glm::inverse(SpiceManager::ref().positionTransformMatrix(_bodyFrame, _mainFrame, _time));

    vecLightSource = _stateMatrix * vecLightSource;

    vecLightSource *= _shadowLength;
    _vertices.clear();

    psc endpoint = psc::CreatePowerScaledCoordinate(vecLightSource.x, vecLightSource.y, vecLightSource.z);
    for (auto v : terminatorPoints){
        _vertices.push_back(CylinderVBOLayout(v[0], v[1], v[2], v[3]));
        glm::vec4 f = psc_addition(v.vec4(), endpoint.vec4());
        _vertices.push_back(CylinderVBOLayout(f[0], f[1], f[2], f[3]));
    }
    _vertices.push_back(_vertices[0]);
    _vertices.push_back(_vertices[1]);

    glBindVertexArray(_vao); // bind array
    glBindBuffer(GL_ARRAY_BUFFER, _vbo); // bind buffer
    glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(CylinderVBOLayout), NULL, GL_DYNAMIC_DRAW); // orphaning the buffer, sending NULL data.
    glBufferSubData(GL_ARRAY_BUFFER, 0, _vertices.size() * sizeof(CylinderVBOLayout), &_vertices[0]);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glBindVertexArray(0);
}

} // namespace openspace
