/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2015                                                                    *
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

// Textures and buffers
uniform sampler2D cubeBack;
uniform sampler3D textureAtlas;
uniform sampler1D transferFunction;
layout (rgba32f, binding = 3) writeonly uniform image2D out_image;

// TSP settings
uniform int     gridType              = 0;
uniform float   stepSize              = 0.002;
uniform uint    maxNumBricksPerAxis   = 0;
uniform int     paddedBrickDim        = 0;

in vec4 vs_position;
in vec4 vs_color;

layout( shared, binding=1 ) buffer Bricks
{
    uint atlasMap[];
};

#define   M_PI      3.14159265358979323846  /* pi */
#define   M_SQRT1_3 0.57735026919           /* 1/sqrt(3) */

float rand(vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 cartesianToSpherical(vec3 _cartesian) {
  // Put cartesian in [-1..1] range first
  vec3 cartesian = vec3(-1.0,-1.0,-1.0) + _cartesian * 2.0f;

  float r = length(cartesian);
  float theta, phi;

  if (r == 0.0) {
    theta = phi = 0.0;
  } else {
    theta = acos(cartesian.z/r) / M_PI;
    phi = (M_PI + atan(cartesian.y, cartesian.x)) / (2.0*M_PI );
  }
  r *= M_SQRT1_3;
  // r = r / sqrt(3.0f);
  // Sampler ignores w component
  return vec3(r, theta, phi);
}

int intCoords(ivec3 vec3Coords, ivec3 spaceDim) {
    return vec3Coords.x + spaceDim.x*vec3Coords.y + spaceDim.x*spaceDim.y*vec3Coords.z;
}

vec3 vec3Coords(uint intCoords, ivec3 spaceDim) {
    vec3 coords = vec3(0.0);
    coords.x = mod(intCoords, spaceDim.y);
    coords.y = mod(intCoords / spaceDim.x, spaceDim.z);
    coords.z = intCoords / spaceDim.x / spaceDim.y;
    return coords;
}

void atlasMapData(ivec3 brickCoords, inout uint atlasIntCoord, inout uint level) {
    int linearBrickCoords = intCoords(brickCoords, ivec3(maxNumBricksPerAxis));
    uint mapData = atlasMap[linearBrickCoords];
    level = mapData >> 28;
    atlasIntCoord = mapData & 0x0FFFFFFF;
}

vec3 atlasCoords(vec3 position) {

    ivec3 brickCoords = ivec3(position * maxNumBricksPerAxis);
    uint atlasIntCoord, level;
    atlasMapData(brickCoords, atlasIntCoord, level);

    float levelDim = float(maxNumBricksPerAxis / pow(2, level));
    vec3 inBrickCoords = mod(position*levelDim, 1.0);

    float scale = paddedBrickDim - 2.0;
    vec3 paddedInBrickCoords = (1.0 + inBrickCoords * scale) / paddedBrickDim;

    ivec3 numBricksInAtlas = textureSize(textureAtlas, 0)/paddedBrickDim;
    vec3 atlasOffset = vec3Coords(atlasIntCoord, numBricksInAtlas);

    return (atlasOffset + paddedInBrickCoords) / vec3(numBricksInAtlas);
}

vec4 raycast(vec3 startPos, vec3 dir, float maxDist) {
    vec4 color = vec4(0.0);
    for (float distance = 0.0; distance < maxDist;) {

        vec3 position = startPos + distance*dir;
        if (gridType == 1) {
            position = cartesianToSpherical(position);
        }
        vec3 sampleCoords = atlasCoords(position);

        float intensity = texture(textureAtlas, sampleCoords);
        vec4 contribution = texture(transferFunction, intensity);
 
        color += (1.0f - color.a)*contribution;

        distance +=  stepSize;
    }   
    return color;
}

void main() {
    // Get coordinates
    const vec2 texSize = textureSize(cubeBack, 0);
    const vec2 texCoord = vec2(gl_FragCoord.x / texSize.x, gl_FragCoord.y / texSize.y);
     vec3 startPos = vs_color.xyz;
    const vec3 endPos = texture(cubeBack, texCoord).xyz;

    // Calculate direction of raycasting
    vec3 dir = endPos - startPos;
    const float maxDist = length(dir);
    dir = normalize(dir);

    vec4 color = raycast(startPos, dir, maxDist);
    imageStore(out_image, ivec2(gl_FragCoord.xy), color);
    discard;
}
