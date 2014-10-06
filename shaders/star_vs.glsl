#version 440
uniform mat4 ViewProjection;
uniform mat4 ModelTransform;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D texture1;

layout(location = 0) in vec4 in_position;
layout(location = 2) in vec3 in_brightness;

out vec3 vs_brightness;

out vec4 psc_position;
out vec4 cam_position;

#include "PowerScaling/powerScaling_vs.hglsl"

void main(){ 
	vs_brightness = in_brightness;
	psc_position  = in_position;
	cam_position  = campos;


	vec4 tmp = in_position;
	vec4 position = pscTransform(tmp, ModelTransform);
	// psc_position = tmp;
	position = view * model * position;
	// gl_Position =  z_normalization(position);
	gl_Position =  position;
	
}