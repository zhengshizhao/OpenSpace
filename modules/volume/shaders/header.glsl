uniform sampler1D transferFunction_#{volume.id};
uniform sampler3D volume_#{volume.id};
uniform float stepSize_#{volume.id};

float getStepSize_#{volume.id}(vec3 samplePos, vec3 dir) {
    return stepSize_#{volume.id};
}

vec4 sampler_#{volume.id}(vec3 samplePos, vec3 dir, float occludingAlpha, inout float maxStepSize) {
    float intensity = texture(volume_#{volume.id}, samplePos).x;
    vec4 contribution = texture(transferFunction_#{volume.id}, intensity);
    maxStepSize = stepSize_#{volume.id};
    return contribution;
}
