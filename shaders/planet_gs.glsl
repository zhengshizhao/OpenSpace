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

out vec4 gs_position;
out vec3 gs_faceNormal;

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
    gs_faceNormal = normalize(cross(p2.xyz-p0.xyz, p1.xyz-p0.xyz));
    ABufferEmitVertex(p0);
    ABufferEmitVertex(p1);
    ABufferEmitVertex(p2);
    EndPrimitive();
}

void subdivide(int levels, vec4 v0, vec4 v1, vec4 v2) {
    float pss = v0.w;
    float radius = length(v0.xyz);    

    vec4 v01 = vec4(radius * normalize(v0.xyz + v1.xyz), pss);
    vec4 v20 = vec4(radius * normalize(v2.xyz + v0.xyz), pss);
    vec4 v12 = vec4(radius * normalize(v1.xyz + v2.xyz), pss);

    emitTriangle(v0, v01, v20);
    emitTriangle(v01, v1, v12);
    emitTriangle(v01, v12, v20);
    emitTriangle(v20, v12, v2);        
}

// Original code from http://prideout.net/blog/?p=61
void main() {
    vec4 v0 = gl_in[0].gl_Position;
    vec4 v1 = gl_in[1].gl_Position;
    vec4 v2 = gl_in[2].gl_Position;

    // subdivide(1, v0, v1, v2);
    emitTriangle(v0, v1, v2);
}