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

#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/interaction/interactionhandler.h>

namespace openspace {

namespace luascriptfunctions {

/**
 * \ingroup LuaScripts
 * setPosition(number, number, number, number):
 * Sets the camera position in power scaled coordinates
 */
int camera_setPosition(lua_State* L) {
    int nArguments = lua_gettop(L);
    if (nArguments != 4)
        return luaL_error(L, "Expected %d arguments, got %d", 4, nArguments);

    const bool xIsNumber = (lua_isnumber(L, -4) != 0);
    const bool yIsNumber = (lua_isnumber(L, -3) != 0);
    const bool zIsNumber = (lua_isnumber(L, -2) != 0);
    const bool wIsNumber = (lua_isnumber(L, -1) != 0);

    if (!xIsNumber) {
        const char* msg = lua_pushfstring(L, "%s expected, got %s",
                                lua_typename(L, LUA_TNUMBER), luaL_typename(L, -4));
        return luaL_error(L, "bad argument #%d (%s)", 1, msg);
    } else if (!yIsNumber) {
        const char* msg = lua_pushfstring(L, "%s expected, got %s",
                                lua_typename(L, LUA_TNUMBER), luaL_typename(L, -3));
        return luaL_error(L, "bad argument #%d (%s)", 1, msg);
    } else if (!zIsNumber) {
        const char* msg = lua_pushfstring(L, "%s expected, got %s",
                                lua_typename(L, LUA_TNUMBER), luaL_typename(L, -2));
        return luaL_error(L, "bad argument #%d (%s)", 1, msg);
    } else if (!wIsNumber) {
        const char* msg = lua_pushfstring(L, "%s expected, got %s",
                                lua_typename(L, LUA_TNUMBER), luaL_typename(L, -1));
        return luaL_error(L, "bad argument #%d (%s)", 1, msg);
    }

    double xValue = lua_tonumber(L, -4);
    double yValue = lua_tonumber(L, -3);
    double zValue = lua_tonumber(L, -2);
    double wValue = lua_tonumber(L, -1);

    // New camera position
    psc position(xValue, yValue, zValue, wValue);

    // Current settings
    const SceneGraphNode* currentFocusNode = OsEng.interactionHandler()->focusNode();
    psc focusPosition = currentFocusNode->worldPosition();

    glm::vec4 cmp = OsEng.renderEngine()->camera()->position().vec4();

    psc camToFocus = focusPosition - position;
    glm::vec3 viewDir = glm::normalize(camToFocus.vec3());
    glm::vec3 cameraView = glm::normalize(OsEng.renderEngine()->camera()->viewDirection());

    // Set camera position
    OsEng.renderEngine()->camera()->setPosition(position);

    float dot = glm::dot(viewDir, cameraView);
    if (dot < 1.f && dot > -1.f) {
        glm::vec3 rotAxis = glm::normalize(glm::cross(viewDir, cameraView));
        float angle = glm::angle(viewDir, cameraView);
        glm::quat q = glm::angleAxis(angle, rotAxis);
        //rotate view to target focus position
        OsEng.renderEngine()->camera()->rotate(q);
    }

    return 0;
}

} // namespace luascriptfunctions

} // namespace openspace
