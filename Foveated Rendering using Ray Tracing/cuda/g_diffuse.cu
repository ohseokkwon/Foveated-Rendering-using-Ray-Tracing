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
rtDeclareVariable(Matrix4x4, mvp, , );
rtDeclareVariable(float2, screen, , );
rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(float3, eye, , );

rtBuffer<ParallelogramLight>     lights;
rtDeclareVariable(float3, light_position, , );

rtDeclareVariable(rtObject, top_object, , );

rtDeclareVariable(float3, up, , );               // global up vector
rtDeclareVariable(float3, cam_target, , );

RT_PROGRAM void diffuse()
{
	const float3 world_shading_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
	const float3 world_geometric_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, geometric_normal));
	const float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);

	const float z1 = rnd(prd_radiance.seed);
	const float z2 = rnd(prd_radiance.seed);

	float3 diffDir; float3 specDir;
	optix::cosine_sample_hemisphere(z1, z2, diffDir);
	specDir = optix::reflect(ray.direction, world_geometric_normal);

	const optix::Onb onb(ffnormal);
	onb.inverse_transform(diffDir);
	const float3 hitpoint = rtTransformPoint(RT_OBJECT_TO_WORLD, front_hit_point);

	prd_radiance.origin = hitpoint;
	prd_radiance.direction = diffDir;

	const float3 Kd = make_float3(tex2D(Kd_map, texcoord.x / Kd_map_scale.x, texcoord.y / Kd_map_scale.y));
	prd_radiance.result *= Kd;

	// my self
	prd_radiance.normal = world_geometric_normal;
	prd_radiance.depth_value = make_float3(length(hitpoint - eye));

#ifndef ÇÉÈ¦Ä«¸Þ¶ó¿ë
	prd_radiance.reproject_uv = compute_reprojection(hitpoint, prev_mvp, screen);
#else
	prd_radiance.reproject_uv = compute_reprojection_pinhole(hitpoint, eye, cam_target, up, screen);
#endif
	 


	// ±¤¿ø ÀÇÇÑ ±×¸²ÀÚ
	// ±¤¿ø ÀÇÇÑ ±×¸²ÀÚ
	unsigned int num_lights = lights.size();
	float3 result = make_float3(0.0f);

	if (num_lights < 1)
		result = make_float3(1.0f);

	bool isShadow = true;
	for (int i = 0; i < num_lights; ++i)
	{
		// Choose random point on light
		ParallelogramLight light = lights[i];
		const float3 light_pos = light_position + light.v1 + light.v2;

		// Calculate properties of light sample (for area based pdf)
		const float  Ldist = length(light_pos - hitpoint);
		const float3 L = normalize(light_pos - hitpoint);
		const float  nDl = dot(ffnormal, L);
		const float  LnDl = dot(light.normal, L);

		// cast shadow ray
		if (nDl > 0.0f && LnDl > 0.0f)
		{
			PerRayData_pathtrace_shadow shadow_prd;
			shadow_prd.inShadow = false;
			// Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
			Ray shadow_ray = make_Ray(hitpoint, L, 2, scene_epsilon, Ldist - scene_epsilon);
			rtTrace(top_object, shadow_ray, shadow_prd);

			isShadow = shadow_prd.inShadow;
			//if (!shadow_prd.inShadow)
			//{
			//	const float A = length(cross(light.v1, light.v2));
			//	// convert area based pdf to solid angle
			//	const float weight = nDl * LnDl * A / (M_PIf * Ldist * Ldist);
			//	result += light.emission * weight;
			//}
		}
	}

	prd_radiance.radiance = make_float3(1-isShadow);
}