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
#include "device_include/commonStructs.h"
#include "device_include/shared_helper_funcs.h"
#include "device_include/shared_helper_math.h"

using namespace optix;

rtDeclareVariable(float3, shading_normal, attribute shading_normal, );
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3, front_hit_point, attribute front_hit_point, );
rtDeclareVariable(float3, texcoord, attribute texcoord, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );
rtDeclareVariable(float, t_hit, rtIntersectionDistance, );

rtTextureSampler<float4, 2> Kd_map;
rtDeclareVariable(float2, Kd_map_scale, , );

rtDeclareVariable(Matrix4x4, prev_mvp, , );
rtDeclareVariable(float2, screen, , );
rtDeclareVariable(float3, eye, , );

rtBuffer<ParallelogramLight>     lights;
rtDeclareVariable(float3, light_position, , );

rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(int, reflection_max_depth, , );

rtDeclareVariable(float, phong_exp, , );
rtDeclareVariable(float3, Ks, , );
rtDeclareVariable(float, importance_cutoff, , );
rtDeclareVariable(float3, reflectivity_n, , );

#if 1
RT_PROGRAM void reflection()
{
	const float3 world_shading_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
	const float3 world_geometric_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, geometric_normal));
	const float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);

	const float3 hitpoint = rtTransformPoint(RT_OBJECT_TO_WORLD, front_hit_point);

	// my self
	prd_radiance.normal = world_geometric_normal;
	prd_radiance.depth_value = make_float3(length(hitpoint - eye));

	const float3 Kd = make_float3(tex2D(Kd_map, texcoord.x / Kd_map_scale.x, texcoord.y / Kd_map_scale.y));

	// 광원 의한 그림자
	unsigned int num_lights = lights.size();
	float3 shadow_result = make_float3(0.0f);

	for (int i = 0; i < num_lights; ++i)
	{
		ParallelogramLight light = lights[i];
		const float z1 = rnd(prd_radiance.seed);
		const float z2 = rnd(prd_radiance.seed);
		const float3 light_pos = light_position + light.v1 * z1 + light.v2 * z2;

		// Calculate properties of light sample (for area based pdf)
		const float  Ldist = length(light_pos - hitpoint);
		const float3 L = normalize(light_pos - hitpoint);
		const float  nDl = dot(ffnormal, L);
		const float  LnDl = dot(light.normal, L);

		if (nDl > 0.0f && LnDl > 0.0f)
		{
			// cast shadow ray
			PerRayData_pathtrace_shadow shadow_prd;
			shadow_prd.attenuation = make_float3(1.0f);
			optix::Ray shadow_ray(hitpoint, L, 2, scene_epsilon, Ldist);
			rtTrace(top_object, shadow_ray, shadow_prd);
			float3 light_attenuation = shadow_prd.attenuation;

			if (fmaxf(light_attenuation) > 0.0f) {
				const float A = length(cross(light.v1, light.v2));

				// convert area based pdf to solid angle
				const float weight = nDl * LnDl * A / (M_PIf * Ldist * Ldist);
				float3 Lc = light.emission * weight * light_attenuation;
				shadow_result += Kd * nDl * Lc;
				//shadow_result += light.emission * weight * light_attenuation;

				float3 H = normalize(L - ray.direction);
				float nDh = dot(ffnormal, H);
				if (nDh > 0)
					shadow_result += Ks * Lc * pow(nDh, phong_exp);
			}
		}
	}
	prd_radiance.reflectance *= Kd * shadow_result;

	float3 result = Kd * shadow_result;
	float3 r = schlick(-dot(ffnormal, ray.direction), reflectivity_n);
	float importance = prd_radiance.importance * optix::luminance(r);

	// reflection ray
	if (importance > importance_cutoff && prd_radiance.depth < reflection_max_depth) {
		PerRayData_radiance refl_prd;
		refl_prd.importance = importance;
		refl_prd.depth = prd_radiance.depth + 1;
		refl_prd.reflectance = make_float3(0.0f); //prd_radiance.reflectance;

		float3 R = reflect(ray.direction, ffnormal);
		optix::Ray refl_ray(hitpoint, R, 1, scene_epsilon);

		rtTrace(top_object, refl_ray, refl_prd);
		result += r * refl_prd.reflectance;// refl_prd.result;
	}

	prd_radiance.result = result;


	//float3 result = make_float3(0.0f);
	////prd_radiance.result = prd_radiance.reflectance * prd_radiance.radiance;

	//// depth 반복
	//if (prd_radiance.done) {
	//	prd_radiance.result += prd_radiance.reflectance;
	//}
	//else if (prd_radiance.depth < reflection_max_depth) {
	//	Ray ray(hitpoint, specDir, 1, scene_epsilon);
	//	PerRayData_radiance refl_prd;
	//	refl_prd.depth = prd_radiance.depth + 1;
	//	refl_prd.result = make_float3(0.0f);
	//	refl_prd.reflectance = prd_radiance.reflectance;

	//	rtTrace(top_object, ray, refl_prd);

	//	//result = refl_prd.result;// refl_prd.reflectance * refl_prd.radiance;
	//	prd_radiance.result += refl_prd.reflectance * refl_prd.result;
	//}
}
#else
RT_PROGRAM void reflection()
{
	const float3 world_shading_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
	const float3 world_geometric_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, geometric_normal));
	const float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);

	float3 specDir;
	const float3 hitpoint = rtTransformPoint(RT_OBJECT_TO_WORLD, front_hit_point);

	prd_radiance.origin = hitpoint;
	prd_radiance.direction = specDir;

	const float3 Kd = make_float3(tex2D(Kd_map, texcoord.x / Kd_map_scale.x, texcoord.y / Kd_map_scale.y));
	prd_radiance.reflectance *= Kd;

	// my self
	prd_radiance.normal = world_geometric_normal;
	prd_radiance.depth_value = make_float3(t_hit);

	// 광원 의한 그림자
	unsigned int num_lights = lights.size();
	float3 shadow_result = make_float3(0.0f);

	if (num_lights < 1)
		shadow_result = make_float3(1.0f);

	for (int i = 0; i < num_lights; ++i)
	{
		ParallelogramLight light = lights[i];
		const float z1 = rnd(prd_radiance.seed);
		const float z2 = rnd(prd_radiance.seed);
		const float3 light_pos = light.corner + light.v1 * z1 + light.v2 * z2;

		// Calculate properties of light sample (for area based pdf)
		const float  Ldist = length(light_pos - hitpoint);
		const float3 L = normalize(light_pos - hitpoint);
		const float  nDl = dot(ffnormal, L);
		const float  LnDl = dot(light.normal, L);

		if (nDl > 0.0f && LnDl > 0.0f)
		{
			// cast shadow ray
			PerRayData_pathtrace_shadow shadow_prd;
			shadow_prd.attenuation = make_float3(1.0f);
			optix::Ray shadow_ray(hitpoint, L, 2, scene_epsilon, Ldist);
			rtTrace(top_object, shadow_ray, shadow_prd);
			float3 light_attenuation = shadow_prd.attenuation;

			if (fmaxf(light_attenuation) > 0.0f) {
				const float A = length(cross(light.v1, light.v2));
				// convert area based pdf to solid angle
				const float weight = nDl * LnDl * A / (M_PIf * Ldist * Ldist);
				shadow_result += light.emission * weight * light_attenuation;
			}
		}
	}
	prd_radiance.radiance = shadow_result;

	prd_radiance.result = shadow_result * Kd;
}
#endif
//-----------------------------------------------------------------------------
//
//  Shadow any-hit
//
//-----------------------------------------------------------------------------

rtDeclareVariable(PerRayData_pathtrace_shadow, current_prd_shadow, rtPayload, );
RT_PROGRAM void shadow()
{
	current_prd_shadow.attenuation = make_float3(0.0f);
	//current_prd_shadow.inShadow = true;
	rtIgnoreIntersection();
	/*rtTerminateRay();

	float3 world_shading_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
	float nDi = fabs(dot(world_shading_normal, ray.direction));

	current_prd_shadow.attenuation *= 1.0f - fresnel_schlick(nDi, 5.0f, 1.0f - shadow_attenuation, make_float3(1.0f));
	current_prd_shadow.inShadow = true;

	rtIgnoreIntersection();*/
}