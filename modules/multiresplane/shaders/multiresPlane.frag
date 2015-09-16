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

#version __CONTEXT__

uniform uint maxNumBricksPerAxis = 8;
uniform ivec2 paddedBrickDim = ivec2(512, 512);
uniform ivec2 bricksInAtlas = ivec2(8, 8);
uniform ivec2 padding = ivec2(1, 1);

uniform sampler2D atlas;
uniform sampler1D transferFunction;

layout (shared) buffer atlasMapBlock {
    uint atlasMap[];
};

in vec4 worldPosition;
in vec2 vUv;

#include "ABuffer/abufferStruct.hglsl"
#include "ABuffer/abufferAddToBuffer.hglsl"
#include "PowerScaling/powerScaling_fs.hglsl"

int intCoord(ivec2 vec2Coords, ivec2 spaceDim) {
    return vec2Coords.x + spaceDim.x*vec2Coords.y;
}

vec2 vec2Coords(uint intCoord, ivec2 spaceDim) {
    vec2 coords = vec2(0.0);
    coords.x = mod(intCoord, spaceDim.x);
    coords.y = intCoord / spaceDim.x;
    return coords;
}

void main() {
    // look up things in atlas map
    ivec2 brickCoords = ivec2(vUv * maxNumBricksPerAxis);
    uint linearBrickCoord = intCoord(brickCoords, ivec2(maxNumBricksPerAxis));
    uint atlasMapValue = atlasMap[linearBrickCoord];
    uint level = atlasMapValue >> 28;
    uint atlasIntCoord = atlasMapValue & 0x0FFFFFFF;
    float levelDim = float(maxNumBricksPerAxis) / pow(2.0, level);
    vec2 atlasOffset = vec2Coords(atlasIntCoord, bricksInAtlas);
    
    // we now have:
    // 1) Level dim (number of most high-res bricks this texture tile should fill)
    // 2) Atlas offset (number of bricks to skip from (0, 0) in the atlas)
    
    vec2 inBrickCoords = mod(vUv*levelDim, 1.0);
    vec2 scale = vec2(paddedBrickDim - 2*padding);
    vec2 paddedInBrickCoords = (padding + inBrickCoords * scale) / paddedBrickDim;
    vec2 atlasCoords = (atlasOffset + paddedInBrickCoords) / bricksInAtlas;

    float intensity = texture(atlas, atlasCoords).r;
    vec4 fragColor = texture(transferFunction, intensity);
    //fragColor = 0.5 * fragColor + 0.5 * vec4(atlasCoords, vec2(0.0, 1.0));

    vec4 position = worldPosition;
    float depth = pscDepth(position);

    gl_FragDepth = depth;

    ABufferStruct_t frag;
    _col_(frag, fragColor);
    _z_(frag, depth);
    _type_(frag, 0);
    _pos_(frag, position);
    addToBuffer(frag);
}
