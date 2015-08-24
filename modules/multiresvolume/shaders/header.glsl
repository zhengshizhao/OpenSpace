uniform sampler1D transferFunction_#{volume.id};
uniform sampler3D textureAtlas_#{volume.id};
uniform int gridType_#{volume.id};
uniform uint maxNumBricksPerAxis_#{volume.id};
uniform uint paddedBrickDim_#{volume.id};
uniform ivec3 nBricksInAtlas_#{volume.id};
uniform ivec3 atlasSize_#{volume.id};

layout (shared) buffer atlasMapBlock_#{volume.id} {
    uint atlasMap_#{volume.id}[];
};

void atlasMapDataFunction_#{volume.id}(ivec3 brickCoords, inout uint atlasIntCoord, inout uint level) {
    int linearBrickCoord = multires_intCoord(brickCoords, ivec3(maxNumBricksPerAxis_#{volume.id}));
    uint mapData = atlasMap_#{volume.id}[linearBrickCoord];
    level = mapData >> 28;
    atlasIntCoord = mapData & 0x0FFFFFFF;
}

vec3 atlasCoordsFunction_#{volume.id}(vec3 position) {
    uint maxNumBricksPerAxis = maxNumBricksPerAxis_#{volume.id};
    uint paddedBrickDim = paddedBrickDim_#{volume.id};

    ivec3 brickCoords = ivec3(position * maxNumBricksPerAxis);
    uint atlasIntCoord, level;
    atlasMapDataFunction_#{volume.id}(brickCoords, atlasIntCoord, level);

    float levelDim = float(maxNumBricksPerAxis) / pow(2.0, level);
    vec3 inBrickCoords = mod(position*levelDim, 1.0);

    float scale = float(paddedBrickDim) - 2.0;
    vec3 paddedInBrickCoords = (1.0 + inBrickCoords * scale) / paddedBrickDim;

    ivec3 numBricksInAtlas = ivec3(vec3(atlasSize_#{volume.id}) / paddedBrickDim);
    vec3 atlasOffset = multires_vec3Coords(atlasIntCoord, numBricksInAtlas);
    return (atlasOffset + paddedInBrickCoords) / vec3(numBricksInAtlas);
}

float getStepSize_#{volume.id}(vec3 samplePos, vec3 dir){
      return 1.0/float(maxNumBricksPerAxis_#{volume.id})/float(paddedBrickDim_#{volume.id});
}

vec4 sampler_#{volume.id}(vec3 samplePos, vec3 dir, float occludingAlpha, inout float maxStepSize) {
    if (gridType_#{volume.id} == 1) {
        samplePos = multires_cartesianToSpherical(samplePos);
    }
    vec3 sampleCoords = atlasCoordsFunction_#{volume.id}(samplePos);
    float intensity = texture(textureAtlas_#{volume.id}, sampleCoords).x;
    vec4 contribution = texture(transferFunction_#{volume.id}, intensity);
    maxStepSize = 1.0/float(maxNumBricksPerAxis_#{volume.id})/float(paddedBrickDim_#{volume.id});
    return contribution;
}
