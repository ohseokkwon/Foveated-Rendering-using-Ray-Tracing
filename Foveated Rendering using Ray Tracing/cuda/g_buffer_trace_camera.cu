/*
* Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*  * Neither the name of NVIDIA CORPORATION nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <optix_cuda.h>
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include "device_include/helpers.h"
#include "device_include/prd.h"
#include "device_include/random.h"
#include "device_include/shared_helper_funcs.h"

using namespace optix;


rtDeclareVariable(float3, prev_eye, , );
rtDeclareVariable(float3, eye, , );
rtDeclareVariable(float, eye_variance, , );
rtDeclareVariable(Matrix4x4, mvp, , );
rtDeclareVariable(Matrix4x4, prev_mvp, , );
rtDeclareVariable(float2, gaze, , );
rtDeclareVariable(float, g_apertureSize, , );

rtDeclareVariable(float4, bad_color, , );
rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(float3, cutoff_color, , );
rtDeclareVariable(float3, bbox_min, , );
rtDeclareVariable(float3, bbox_max, , );

rtBuffer<float4, 2>              position_buffer;
rtBuffer<float4, 2>              normal_buffer;
rtBuffer<float4, 2>              depth_buffer;
rtBuffer<float4, 2>              diffuse_buffer;
rtBuffer<float4, 2>              weight_buffer;

rtBuffer<float4, 2>              history_buffer;
rtBuffer<float4, 2>              history_cache;

rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(unsigned int, frame, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );

rtBuffer<float3, 1> gaze_target;
rtDeclareVariable(float3, up, , );               // global up vector
rtDeclareVariable(float3, cam_target, , );


__device__ void d_buffer_init() {
	normal_buffer[launch_index] = make_float4(0.0f);
	position_buffer[launch_index] = make_float4(0.0f);
	depth_buffer[launch_index] = make_float4(0.0f);
	diffuse_buffer[launch_index] = make_float4(0.0f);
	weight_buffer[launch_index] = make_float4(0.0f);
	
	history_buffer[launch_index] = make_float4(0.0f);
	history_cache[launch_index] = make_float4(0.0f);
}

RT_PROGRAM void g_buffer_trace()
{
	if (frame < 1) {
		d_buffer_init();
	}
	size_t2 screen = position_buffer.size();
	float2 screenf = make_float2(screen);

	uint seed = tea<16>(screen.x*launch_index.y + launch_index.x, 0);
	//thread_cache[launch_index] = make_uint3(0);

	float4 tmp = make_float4(make_float2(launch_index) / screenf * 2.0f - 1.0f, -1.0f, 1.0f);
	tmp = mvp * tmp;
	float3 nearPos = make_float3(tmp) / tmp.w;

	float3 ray_origin = eye;
	float3 ray_direction = normalize(nearPos - eye);

#define 핀홀카메라용
	/*uint seed0 = launch_index.x + seed;
	uint seed1 = launch_index.y + seed;
	float2 pixel = make_float2((float)launch_index.x / screenf.x, (float)(screenf.y - launch_index.y) / screenf.y);
	createPinHoleCam(eye, cam_target, up, gaze_target[0], pixel, ray_origin, ray_direction, g_apertureSize, seed0, seed1);*/

	PerRayData_radiance prd;
	prd.result = make_float3(1.0f);
	prd.depth = 0;
	prd.seed = seed;
	prd.done = false;

	// These represent the current shading state and will be set by the closest-hit or miss program

	// attenuation (<= 1) from surface interaction.
	prd.reflectance = make_float3(1.0f);

	// light from a light source or miss program
	//prd.radiance = make_float3(1.0f);

	// next ray to be traced
	prd.normal = make_float3(0.0f);
	prd.depth_value = make_float3(0.0f);
	prd.reproject_uv = make_float2(-1.0f);

	float3 result = make_float3(0.0f);

	// Main render loop. This is not recursive, and for high ray depths
	// will generally perform better than tracing radiance rays recursively
	// in closest hit programs.

	// 레이 트레이스
	optix::Ray ray(ray_origin, ray_direction, /*ray type*/ 0, scene_epsilon);
	rtTrace(top_object, ray, prd);

	result += prd.result;// *prd.radiance;
	//result += prd.reflectance * cutoff_color;
	if (prd.done) {
		result += prd.result;// *prd.radiance;
	}

	// tone mapping
	//result = Uncharted2ToneMapping(result);
	position_buffer[launch_index] = make_float4(prd.origin, 1.0f);
	//normal_buffer[launch_index] = make_float4(prd.normal * 0.5f + 0.5f, 1.0f);
	normal_buffer[launch_index] = make_float4(prd.normal * 0.5f + 0.5f, prd.radiance.x);
	depth_buffer[launch_index] = make_float4(prd.depth_value, 1.0f);
	diffuse_buffer[launch_index] = make_float4(result, 1.0f);
	weight_buffer[launch_index] = make_float4(prd.reproject_uv, 0.0f, 1.0f);
}

RT_PROGRAM void exception()
{
	const unsigned int code = rtGetExceptionCode();
	rtPrintf("Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y);
	diffuse_buffer[launch_index] = bad_color;
}

