#pragma once

#include <optix_cuda.h>
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>

static __device__ __inline__ float3 exp(const float3& x)
{
	return make_float3(exp(x.x), exp(x.y), exp(x.z));
}

static __device__ __inline__ float3 schlick(float nDi, const float3& rgb)
{
	float r = fresnel_schlick(nDi, 5, rgb.x, 1);
	float g = fresnel_schlick(nDi, 5, rgb.y, 1);
	float b = fresnel_schlick(nDi, 5, rgb.z, 1);
	return make_float3(r, g, b);
}


static __device__ __inline__ float get_random(uint seed0, uint seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	seed0 = 36969 * (seed0 & 65535) + (seed0 >> 16);
	seed1 = 18000 * (seed1 & 65535) + (seed1 >> 16);

	unsigned int ires = (seed0 << 16) + seed1;

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}