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
rtDeclareVariable(float, g_apectureSize, , );
rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(float3, bbox_min, , );
rtDeclareVariable(float3, bbox_max, , );
rtDeclareVariable(int, max_depth, , );

rtBuffer<float4, 2>              position_buffer;
rtBuffer<float4, 2>              depth_buffer;
rtBuffer<float4, 2>              depth_cache;
rtBuffer<float4, 2>              weight_buffer;

rtBuffer<float4, 2>              normal_buffer;
rtBuffer<float4, 2>              diffuse_buffer;

rtBuffer<uint3, 2>				 thread_buffer;
rtBuffer<uint3, 2>				 thread_cache;
rtBuffer<float4, 2>              extra_buffer;

rtBuffer<float3, 1> gaze_target;

rtTextureSampler<float4, 2> sampling_map;
rtDeclareVariable(unsigned int, frame, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );


RT_PROGRAM void sampling_step()
{
	size_t2 screen = thread_buffer.size();
	float2 screenf = make_float2(screen);

	unsigned int seed = tea<16>(screen.x*launch_index.y + launch_index.x, frame);

	bool usingRay = false;

	// 초기화
	thread_cache[launch_index] = make_uint3(0);

	// 엣지 찾기
//	float isBoundary = edge_detect(depth_buffer, launch_index, screenf);

	//float4 tmp = make_float4(make_float2(launch_index) / screenf - 0.5f, 0.0f, 1.0f);
	//tmp = mvp * tmp;
	//float3 nearPos = make_float3(tmp) / tmp.w;

	//float3 ray_origin = eye;
	//float3 ray_direction = normalize(nearPos - eye);

	float4 position = position_buffer[launch_index];
	float4 depth = depth_buffer[launch_index];

	float4 weight = weight_buffer[launch_index];
	float2 query_uv = make_float2(weight.x, weight.y);
	uint2 q_uv = launch_index;

	float isValid = 0.0f; float isEdge = 0.0f; float s_depth_grad = 0.0f;
	if (query_uv.x > -1.0f && query_uv.y > -1.0f) {
		if ((0 <= query_uv.x && query_uv.x < screenf.x - 0.5f) &&
			(0 <= query_uv.y && query_uv.y < screenf.y - 0.5f)) {
			
			q_uv = make_uint2(round(query_uv.x), round(query_uv.y));

			float4 prev_depth = depth_cache[q_uv];
			
			//prev_depth = bilinear(depth_cache, query_uv, screenf);
			//position = bilinear(position_buffer, query_uv, screenf);
			//float4 tmp = make_float4(query_uv / screenf - 0.5f, 0.0f, 1.0f);
			//tmp = prev_mvp.inverse() * tmp;
			//float3 nearPos = make_float3(tmp) / tmp.w;

			//float diff = (prev_depth.x - length(make_float3(position) - prev_eye));
			//diff = pow(diff, 2.0f);
			//diff = abs(diff);

			//s_depth_grad = gradient(depth_cache, launch_index, screenf);

			float depth_epsilon = scene_epsilon; /* obj 파일별로 depth 최소 변화량이 선택되어야함 */
			isValid = isCacheHit(prev_depth.x, length(make_float3(position) - prev_eye), depth_epsilon);
			//isValid = (abs(diff) < depth_epsilon ? 1.0f : 0.0f);

			//float4 prev_normal = normal_cache[q_uv];
			//float edge = max(dot(make_float3(prev_normal), normalize(-ray_direction) * 0.5f + 0.5f), 0.0f);
			//isEdge = edge < scene_epsilon ? 1.0f : 0.0f;
			//if (edge < scene_epsilon)
			//	isValid = 0.0f;

			//// 캐시 미스
			//if (history_cache[q_uv].w < 1.0f) {
			//	isValid = 0.0f;
			//}
			//if (isBoundary)
			//	isValid = 0.0f;

			// 실험용 1번만 샘플링 (안할땐 주석처리)
			//isValid = 0.0f;
		}
	}
	
	// My Equation
	float gaze_dist = length(make_float2(launch_index) - gaze) / length(screenf);
	float alpha = ((1.0f / 0.8f) - 1.0f) / pow(g_apectureSize, 2);
	float sample_rate = clamp((1.0f / (alpha * pow(2 * gaze_dist, 2) + 1)), 0.0f, 1.0f);
	
	// Weier et al's Equation
	float p_min = 0.05f, r0 = g_apectureSize, r1 = g_apectureSize * 2.0f;
	sample_rate = gaze_dist < r0 ? 
		1.0f : 
		(gaze_dist > r1 ? 
			p_min : 
			(1.0f-(1.0f - p_min) * ((gaze_dist - r0) / (r1 - r0)))
		);

	//// Fujita et al's Equation
	//sample_rate = pow(gaze_dist*length(screenf) / 300.0f, -2.0f / 3.0f);
	
	// 리프로젝션의 결과가 없다면 레이를 쏜다. (usingRay && !isValid || isBoundary)
	//bool usingRay = (rnd(seed) < sample_rate) && (true ? rnd(seed) < 0.5f : (!isValid)) || isBoundary;
	//usingRay = (rnd(seed) < sample_rate);

	// 체크보드 패턴으로 샘플링
	/*usingRay = true;
	if ((launch_index.x / 2) % 2 == 0 && (launch_index.y / 2) % 2 == 0)
		usingRay = false;*/

	// 가로선 방향으로 샘플링
	//usingRay = true;
	//if ((launch_index.x / 50) % 2 == 0)
	//	usingRay = false;
	
	

	

	// 로그폴라 기반 샘플링
	/*uint2 uv = FowardLogPolar(launch_index, gaze, screenf * 0.25f);
	uint2 xy = InverseLogPolar(uv, gaze, screenf * 0.25f);
	usingRay = length(make_float2(launch_index - xy)) < sqrt(length(make_float2(1.5f))) ? true : false;*/

	gaze_target[0] = make_float3(position_buffer[make_uint2(gaze)]);

	int scale = 4;
	uint2 sampling_uv = make_uint2(scale * (launch_index.x / scale), 
		scale * (launch_index.y / scale));
	uint2 scale_launch_size = make_uint2(screen.x / scale, screen.y / scale);

	float3 RG_BY_L = RGBY(diffuse_buffer, sampling_uv);
	//float s_intensity = luminance(diffuse_buffer, launch_index, sampling_uv);
	float gx = gradient_x(diffuse_buffer, sampling_uv, screenf, scale);
	float gy = gradient_y(diffuse_buffer, sampling_uv, screenf, scale);
	float s_orientation = orientation_by_sobel(gx, gy);

	float s_depth = depth_saliency(depth_buffer, sampling_uv, make_uint2(gaze), length(bbox_max - bbox_min) * 0.005f);
	float s_shadow = normal_buffer[sampling_uv].w;
	float s_normal_grad = gradient(normal_buffer, sampling_uv, screenf, scale);
	
	float velocity = length(make_float2(launch_index) - query_uv) * 0.5f;
	if (query_uv.x < 0.0f && query_uv.y < 0.0f)
		velocity = 0.0f;
	float s_velocity = velocity_map(velocity);
	

	float saliency = 0.0f;// max(gradient(normal_buffer, sampling_uv, screenf / scale, scale), gradient(diffuse_buffer, sampling_uv, screenf / scale, scale));
	/*saliency = max(saliency, RG_BY_L.x);
	saliency = max(saliency, RG_BY_L.y);
	saliency = max(saliency, RG_BY_L.z);
	saliency = max(saliency, s_orientation);*/
	//saliency = max(saliency, s_intensity);
	//saliency = max(saliency, s_normal);
	//saliency = max(saliency, gradient_y(diffuse_buffer, sampling_uv, screenf / scale, scale));

	saliency = ((RG_BY_L.x + RG_BY_L.y) / 2.0f + RG_BY_L.z + s_orientation) / 3.0f;
	saliency = max(saliency, s_normal_grad);
	saliency *= s_depth;
	saliency = max(saliency, s_velocity) * s_shadow;

	// 마스크 기반 샘플링
	usingRay = masked_sampling(launch_index, gaze_dist, g_apectureSize, saliency);


	// Weier 샘플링 + saliency
	/*uint k = tea<16>(screen.x*launch_index.y + launch_index.x, 0);
	bool sample_rate_1 = rnd(k) < sample_rate;
	bool sample_rate_2 = rnd(k) < saliency;
	usingRay = sample_rate_1 | sample_rate_2;*/

	thread_buffer[launch_index] = make_uint3(launch_index, usingRay);
	weight_buffer[launch_index] = make_float4(query_uv, isValid, 0.0f);
	//extra_buffer[launch_index] = make_float4(heatmap(saliency), 1.0f);

	//extra_buffer[launch_index] = make_float4(query_uv / screenf, 0.0f, 1.0f);

	//extra_buffer[launch_index] = make_float4(make_float3(usingRay), 1.0f);
	//weight_buffer[launch_index] = make_float4(heatmap(saliency), 0.0f);
}