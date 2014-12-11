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

#version __CONTEXT__

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 ViewProjection;
uniform mat4 ModelTransform;

in vec2 vs_toastCoord[];

out vec4 gs_position;
out vec2 gs_toastCoord;
// out vec3 gs_faceNormal;

#include "PowerScaling/powerScaling_vs.hglsl"

// Calculate the correct powerscaled position and depth for the ABuffer
void ABufferEmitVertex(vec4 pos) {
    // calculate psc position
    vec4 tmp = pos;
    vec4 position = pscTransform(tmp, ModelTransform);
    gs_position = tmp;

    // project the position to view space
    position =  ViewProjection*position;
    gl_Position = position;
    EmitVertex();
}

void emitTriangle(vec4 p0, vec4 p1, vec4 p2) {
    // gs_faceNormal = normalize(cross(p2.xyz-p0.xyz, p1.xyz-p0.xyz));
    
    gs_toastCoord = vs_toastCoord[0];
    ABufferEmitVertex(p0);

    gs_toastCoord = vs_toastCoord[1];
    ABufferEmitVertex(p1);

    gs_toastCoord = vs_toastCoord[2];
    ABufferEmitVertex(p2);
    EndPrimitive();
}

// Original code from http://prideout.net/blog/?p=61
void main() {
    vec4 v0 = gl_in[0].gl_Position;
    vec4 v1 = gl_in[1].gl_Position;
    vec4 v2 = gl_in[2].gl_Position;

    emitTriangle(v0, v1, v2);
}