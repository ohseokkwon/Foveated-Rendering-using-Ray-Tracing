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

rtDeclareVariable(float3, cutoff_color, , );
rtDeclareVariable(int, max_depth, , );

rtBuffer<ParallelogramLight>     lights;
rtDeclareVariable(float, scene_epsilon, , );
rtDeclareVariable(rtObject, top_object, , );

rtDeclareVariable(unsigned int, frame, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, launch_dim, rtLaunchDim, );

/// Ãß°¡
rtDeclareVariable(PerRayData_pathtrace_shadow, prd_shadow, rtPayload, );

rtDeclareVariable(unsigned int, radiance_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type, , );

rtDeclareVariable(float, fresnel_exponent, , );
rtDeclareVariable(float, fresnel_minimum, , );
rtDeclareVariable(float, fresnel_maximum, , );
rtDeclareVariable(float, refraction_index, , );
rtDeclareVariable(int, refraction_maxdepth, , );
rtDeclareVariable(int, reflection_maxdepth, , );
rtDeclareVariable(float3, refraction_color, , );
rtDeclareVariable(float3, reflection_color, , );
rtDeclareVariable(float3, extinction_constant, , );

rtDeclareVariable(float, importance_cutoff, , );
rtDeclareVariable(float3, shadow_attenuation, , );

RT_PROGRAM void refraction()
{
	// intersection vectors
	const float3 h = ray.origin + t_hit * ray.direction;            // hitpoint
	const float3 n = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal)); // normal
	const float3 i = ray.direction;                                            // incident direction

	const float3 Kd = make_float3(tex2D(Kd_map, texcoord.x / Kd_map_scale.x, texcoord.y / Kd_map_scale.y));
	//prd_radiance.reflectance = Kd;

	// my self
	prd_radiance.normal = n;
	prd_radiance.depth_value = make_float3(t_hit);

	float reflection = 1.0f;
	float3 result = make_float3(0.0f);

	float3 beer_attenuation;
	if (dot(n, ray.direction) > 0) {
		// Beer's law attenuation
		beer_attenuation = exp(extinction_constant * t_hit);
	}
	else {
		beer_attenuation = make_float3(1);
	}

	// refraction
	if (prd_radiance.depth < min(refraction_maxdepth, max_depth))
	{
		float3 t;                                                            // transmission direction
		if (refract(t, i, n, refraction_index))
		{

			// check for external or internal reflection
			float cos_theta = dot(i, n);
			if (cos_theta < 0.0f)
				cos_theta = -cos_theta;
			else
				cos_theta = dot(t, n);

			reflection = fresnel_schlick(cos_theta, fresnel_exponent, fresnel_minimum, fresnel_maximum);

			float importance = prd_radiance.importance * (1.0f - reflection) * optix::luminance(refraction_color * beer_attenuation);
			if (importance > importance_cutoff) {
				optix::Ray ray(h, t, 1, scene_epsilon);
				PerRayData_radiance refr_prd;
				refr_prd.depth = prd_radiance.depth + 1;
				refr_prd.importance = importance;

				rtTrace(top_object, ray, refr_prd);
				result += (1.0f - reflection) * refraction_color * refr_prd.result;
			}
			else {
				result += (1.0f - reflection) * refraction_color * cutoff_color;
			}
		}
		// else TIR
	}

	// reflection
	if (prd_radiance.depth < min(reflection_maxdepth, max_depth))
	{
		float3 r = reflect(i, n);

		float importance = prd_radiance.importance * reflection * optix::luminance(reflection_color * beer_attenuation);
		if (importance > importance_cutoff) {
			optix::Ray ray(h, r, 1, scene_epsilon);
			PerRayData_radiance refl_prd;
			refl_prd.depth = prd_radiance.depth + 1;
			refl_prd.importance = importance;

			rtTrace(top_object, ray, refl_prd);
			result += reflection * reflection_color * refl_prd.result;
		}
		else {
			result += reflection * reflection_color * cutoff_color;
		}
	}

	result = result * beer_attenuation;

	prd_radiance.result = Kd * result;
	prd_radiance.done = true;
}

RT_PROGRAM void any_hit_shadow()
{
	float3 world_shading_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
	float nDi = fabs(dot(world_shading_normal, ray.direction));

	prd_shadow.attenuation *= 1.0f - fresnel_schlick(nDi, 5.0f, 1.0f - shadow_attenuation, make_float3(1.0f));
	//prd_shadow.inShadow = true;

	rtIgnoreIntersection();
}

RT_PROGRAM void exception()
{
	const unsigned int code = rtGetExceptionCode();
	rtPrintf("Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y);
	//shading_buffer[launch_index] = bad_color;
}
