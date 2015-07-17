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

// ================================================================================
// Settings
// ================================================================================
//#pragma openspace insert SETTINGS
//#include <${SHADERS_GENERATED}/ABufferSettings.hglsl>:notrack

// Select type of depth calculations
#define 	PSCDEPTH 		1
#define 	ZDEPTH 			2
#define 	ZTYPE 			ZDEPTH

// Maximum number of fragments
#ifdef MAX_LAYERS
#define 	MAX_FRAGMENTS 	MAX_LAYERS
#else
#define 	MAX_FRAGMENTS 	16 				// The size of the local fragment list
#endif

#define 	USE_JITTERING					//
// #define 	USE_COLORNORMALIZATION			//

// If you need to render a volume box but not sample the volume (debug purpose)
// #define 	SKIP_VOLUME_0
// #define 	SKIP_VOLUME_1
// #define 	SKIP_VOLUME_2
// #define 	SKIP_VOLUME_3

// constants
const float samplingRate = 1.0;
uniform float ALPHA_LIMIT = 0.99;

uniform float blackoutFactor = 0.0;


// Math defintions
#define 	M_E   		2.7182818284590452354
#define 	M_LOG2E   	1.4426950408889634074 	/* log_2 e */
#define 	M_LOG10E   	0.43429448190325182765 	/* log_10 e */
#define 	M_LN2   	0.69314718055994530942 	/* log_e 2 */
#define 	M_LN10   	2.30258509299404568402 	/* log_e 10 */
#define 	M_PI   		3.14159265358979323846 	/* pi */
#define 	M_PI_2   	1.57079632679489661923 	/* pi/2 */
#define 	M_PI_4   	0.78539816339744830962 	/* pi/4 */
#define 	M_1_PI   	0.31830988618379067154 	/* 1/pi */
#define 	M_2_PI   	0.63661977236758134308 	/* 2/pi */
#define 	M_2_SQRTPI	1.12837916709551257390 	/* 2/sqrt(pi) */
#define 	M_SQRT2 	1.41421356237309504880 	/* sqrt(2) */
#define 	M_SQRT1_2   0.70710678118654752440 	/* 1/sqrt(2) */
#define		M_SQRT1_3	0.57735026919			/* 1/sqrt(3) */

in vec2 texCoord;
out vec4 out_color;

// ================================================================================ 
// Helpers,
// Module specific functions required by samplers
// ================================================================================
#include <${SHADERS_GENERATED}/ABufferHelpers.hglsl>:notrack

// ================================================================================ 
// Headers, 
// volume and transferfunctions uniforms
// ================================================================================
//#pragma openspace insert HEADERS
#include <${SHADERS_GENERATED}/ABufferHeaders.hglsl>:notrack

// ================================================================================
// The ABuffer specific includes and definitions
// ================================================================================
#include "abufferStruct.hglsl"
ABufferStruct_t fragments[MAX_FRAGMENTS];

#if MAX_VOLUMES > 0
	vec3 volume_direction[MAX_VOLUMES];
	vec3 volume_position[MAX_VOLUMES];
	int volumes_in_fragment[MAX_VOLUMES];
	float volume_scale[MAX_VOLUMES];
	int volume_count = 0;
#endif
#include "../PowerScaling/powerScalingMath.hglsl"
#include "abufferSort.hglsl"

// ================================================================================
// Helper functions functions
// ================================================================================

vec4 blend(vec4 front, vec4 back) {
    vec4 result;
    result.a = front.a + (1.0 - front.a) * back.a;
    result.rgb = (front.rgb * front.a) + (back.rgb * back.a * (1.0 - front.a));
    result.rgb = result.a > 0.000001 ? (result.rgb / result.a) : result.rgb;
    return result;
}

void blendStep(inout vec4 dst, in vec4 src, in float stepSize) {
    src.a = 1.0 - pow(1.0 - src.a, stepSize);
    dst = blend(dst, src);
}

float rand(vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}


vec4 calculate_final_color(uint frag_count) {

	int currentVolumeBitmask = 0;
	vec4 final_color = vec4(0);

	if(frag_count == 1 && _type_(fragments[0]) == 0) {
		final_color = blend(final_color, _col_(fragments[0]));
		return final_color;
	}

	int frag_count_1 = int(frag_count)-1;
	for(int i = 0; i < frag_count_1 && final_color.a < ALPHA_LIMIT; i++) {

		ABufferStruct_t startFrag = fragments[i];
		ABufferStruct_t endFrag = fragments[i+1];
		int type = int(_type_(startFrag));

		if(type == 0) {
			final_color = blend(final_color, _col_(startFrag));
		} else {
			currentVolumeBitmask = currentVolumeBitmask ^ (1 << (type-1));
		}

#if MAX_VOLUMES > 0

		if (currentVolumeBitmask > 0) {

		    vec4 startPos = _pos_(startFrag);
		    vec4 endPos = _pos_(endFrag);
		    vec2 pscDistance = psc_subtraction(startPos.zw, endPos.zw);
		    float fragDistance = abs(pscDistance.x) * pow(k, pscDistance.y);

		    float maxStepSizeLocal; //maximum step size in local scale
		    float maxStepSize; // maxStepSizeLocal converted to global scale
		    float nextStepSize = 10000000000000000.0; // global next step size, minimum maxStepSize
		    float stepSize; // global step size, taken from previous nextStepSize
		    float stepSizeLocal;  // stepSize converted to local scale
		    float jitterFactor = rand(gl_FragCoord.xy); // should be between 0.5 and 1.0


// #pragma openspace insert SAMPLERCALLS
#include <${SHADERS_GENERATED}/ABufferStepSizeCalls.hglsl>:notrack

			float zPosition = 0.0;

			for(int k = 0; final_color.a < ALPHA_LIMIT && k < LOOP_LIMIT; ++k) {

			    stepSize = nextStepSize;
			    zPosition += stepSize;
			    nextStepSize = fragDistance - zPosition;

			    if (nextStepSize < fragDistance/10000.0) {
				break;
			    }

// #pragma openspace insert SAMPLERCALLS
#include <${SHADERS_GENERATED}/ABufferSamplerCalls.hglsl>:notrack

			}
		}
#endif

		
		if(i == frag_count_1 -1 && _type_(endFrag) == 0) {
		    final_color = blend(final_color, _col_(endFrag));
		}


	}

// ================================================================================
// Transferfunction visualizer
// ================================================================================

#ifdef USE_COLORNORMALIZATION
	final_color.rgb = final_color.rgb * final_color.a;
	final_color.a = 1.0;
#endif

	return final_color;

}

// ================================================================================
// Main function
// ================================================================================
void main() {
    out_color = vec4(texCoord,0.0,1.0);
    int frag_count = build_local_fragments_list();
    sort_fragments_list(frag_count);
    out_color = blackoutFactor * calculate_final_color(frag_count);
}

// ================================================================================
// 	The samplers implementations
// ================================================================================
//#pragma openspace insert SAMPLERS

#include <${SHADERS_GENERATED}/ABufferStepSizeFunctions.hglsl>:notrack
#include <${SHADERS_GENERATED}/ABufferSamplers.hglsl>:notrack



