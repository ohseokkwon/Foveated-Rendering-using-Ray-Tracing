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

rtDeclareVariable(float3, eye, , );
rtDeclareVariable(Matrix4x4, mvp, , );

rtDeclareVariable(float4, bad_color, , );
rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(float3, cutoff_color, , );
rtDeclareVariable(int, max_depth, , );
rtDeclareVariable(float2, gaze, , );

rtBuffer<float4, 2>              shading_buffer;

rtBuffer<float4, 2>              history_buffer;
rtBuffer<float4, 2>              history_cache;
rtBuffer<float4, 2>              weight_buffer;
rtBuffer<float4, 2>              depth_buffer;

rtBuffer<uint3, 2>				 thread_buffer;
rtBuffer<uint3, 2>				 thread_cache;

rtBuffer<float4, 2>              extra_buffer;

rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(unsigned int, frame, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, launch_dim, rtLaunchDim, );

rtBuffer<float3, 1> gaze_target;
rtDeclareVariable(float3, up, , );               // global up vector
rtDeclareVariable(float3, cam_target, , );
rtDeclareVariable(float, g_apertureSize, , );
rtDeclareVariable(int, diffuse_max_depth, , );

RT_PROGRAM void ray_trace()
{
	size_t2 screen = shading_buffer.size();
	float2 screenf = make_float2(screen);

	uint3 thread_work = thread_buffer[launch_index];
	uint2 thread_uv = make_uint2(thread_work);

	float3 result = make_float3(0.0f);
	float4 c_history = make_float4(0.0f);

	float4 c_weight = weight_buffer[thread_uv];
	uint2 query_uv = make_uint2(round(c_weight.x), round(c_weight.y));

	if (c_weight.z > 0.0f) {
		// 가우시안 블러 (인접 픽셀로)
		//c_history += gaussian_blur(history_cache, query_uv, screenf);
		c_history = history_cache[query_uv];

		/*float2 query_uv2 = make_float2(c_weight.x, c_weight.y);
		c_history = bilinear(history_cache, query_uv2, screenf);*/
	}

	bool usingRay = thread_work.z;

	//accum_buffer[launch_index] = make_float4(make_float3(thread_work.z), 1.0f);
	//accum_buffer[launch_index] = make_float4(make_float2(thread_uv) / screenf, thread_work.z, 1.0f);
	//accum_buffer[thread_uv] = make_float4(make_float2(c_weight) / screenf * c_weight.z, c_weight.z, 1.0f);
	//accum_buffer[launch_index] = make_float4(make_float2(launch_index) / make_float2(launch_dim), 0.0f, 1.0f);

	if (!usingRay) {
		history_buffer[thread_uv] = c_history;

		shading_buffer[thread_uv] = color_to_accumulated(c_history);

		return;
	}

	// Main render loop. This is not recursive, and for high ray depths
	// will generally perform better than tracing radiance rays recursively
	// in closest hit programs.
	float3 normal = make_float3(0, 0, 0);
	float3 origin = make_float3(0, 0, 0);
	int depth = 0;

	int sqrt_num_samples = 1;
	float2 jitter_scale = 1.0f / screenf / sqrt_num_samples;
	unsigned int samples_per_pixel = sqrt_num_samples*sqrt_num_samples;

	do
	{
		uint seed = tea<16>(screen.x*thread_uv.y + thread_uv.x, c_history.w > 0 ? frame : 0);

		float2 pixel = make_float2(thread_uv) / screenf * 2.0f - 1.0f;
		uint x = samples_per_pixel % sqrt_num_samples;
		uint y = samples_per_pixel / sqrt_num_samples;
		float2 jitter = make_float2(x - rnd(seed), y - rnd(seed));
		float2 d = pixel + jitter*jitter_scale;

		float4 tmp = make_float4(d, -1.0f, 1.0f);
		tmp = mvp * tmp;
		float3 nearPos = make_float3(tmp) / tmp.w;

		float3 ray_origin = eye;
		float3 ray_direction = normalize(nearPos - eye);

#define 핀홀카메라용
		/*uint seed0 = thread_uv.x * (c_history.w > 0 ? frame : 0) + seed;
		uint seed1 = thread_uv.y * (c_history.w > 0 ? frame : 0) + seed;
		pixel = make_float2((float)thread_uv.x / screenf.x, (float)(screenf.y - thread_uv.y) / screenf.y);
		createPinHoleCam(eye, cam_target, up, gaze_target[0], pixel, ray_origin, ray_direction, g_apertureSize, seed0, seed1);*/


		// Ray 정의
		PerRayData_radiance prd;
		prd.result = make_float3(0.0f);
		prd.depth = 0;
		prd.seed = seed;
		prd.done = false;
		prd.importance = 1.0f;

		prd.reflectance = make_float3(1.0f);
		// prd.radiance = make_float3( 1.0f );

		// 레이 트레이스
		optix::Ray ray(ray_origin, ray_direction, /*ray type*/ 1, scene_epsilon);
		rtTrace(top_object, ray, prd);
		result += prd.result;
		seed = prd.seed;

		depth = prd.depth;

	} while (--samples_per_pixel);
	result /= float(sqrt_num_samples*sqrt_num_samples);

	{
		// 톤맵핑
		result = Uncharted2ToneMapping(result);

		float4 final_result = make_float4(result, 1.0f) + c_history;
		history_buffer[thread_uv] = final_result;

		shading_buffer[thread_uv] = color_to_accumulated(final_result);
	}
}

RT_PROGRAM void exception()
{
	const unsigned int code = rtGetExceptionCode();
	rtPrintf("Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y);
	//shading_buffer[launch_index] = bad_color;
}

