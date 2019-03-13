#pragma once

#include <optix_cuda.h>
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>

#include "shared_helper_math.h"

__device__ __constant__ float gaussian_weight[9] = {
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f,
	1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 8.0f,
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f
};
__device__ __constant__ float gradient_weight[9] = {
	-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f,
	-1.0f / 8.0f,  1.0f		  , -1.0f / 8.0f,
	-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f
};
__device__ __constant__ uint2 offset[9] = {
	(-1, +1), (0, +1), (+1, +1),
	(-1, +0), (0, +0), (+1, +0),
	(-1, -1), (0, -1), (+1, -1)
};

__device__ float4 gaussian_blur(optix::buffer<float4, 2> &buffer, const uint2 launch_uv, const float2 bufferSize) {
	float4 result = make_float4(0.0f);
	for (int i = 0; i < 9; i++) {
		uint2 kernel_uv = launch_uv + offset[i];
		if (kernel_uv.x < 0 || kernel_uv.x >= bufferSize.x || kernel_uv.y < 0 || kernel_uv.y >= bufferSize.y)
			continue;
		result += buffer[kernel_uv] * gaussian_weight[i];
	}
	return result;
}

__device__ __inline__ float4 bilinear(optix::buffer<float4, 2> &buffer, const float2 launch_uv, const float2 bufferSize) {
	float4 result = make_float4(0.0f);

	float4 x00 = make_float4(0.0f);
	float4 x01 = make_float4(0.0f);
	float4 x10 = make_float4(0.0f);
	float4 x11 = make_float4(0.0f);
	float u = 0.5f, float v = 0.5f;

	if (0.0f <= launch_uv.x && launch_uv.x < bufferSize.x && 0.0f <= launch_uv.y && launch_uv.y < bufferSize.y) {
		x00 = buffer[make_uint2(floorf(launch_uv.x), floorf(launch_uv.y))];
		x01 = buffer[make_uint2(floorf(launch_uv.x), ceilf(launch_uv.y))];
		x10 = buffer[make_uint2(ceilf(launch_uv.x), floorf(launch_uv.y))];
		x11 = buffer[make_uint2(ceilf(launch_uv.x), ceilf(launch_uv.y))];

		u = ceilf(launch_uv.x) - launch_uv.x;
		v = ceilf(launch_uv.y) - launch_uv.y;

		result = lerp(lerp(x00, x10, u), lerp(x01, x11, u), v);
	}
	return result;
}

__device__ __inline__ bool isCacheHit(const float prev_depth, const float curr_depth, const float epsilon) {
	float diff = prev_depth - curr_depth;

	return abs(diff) < epsilon ? true : false;
}

__device__ float3 RGBY(optix::buffer<float4, 2> &buffer, const uint2 launch_uv) {
	float4 rgba = buffer[launch_uv];
	float R = rgba.x - (rgba.y + rgba.z) / 2.0f;
	float G = rgba.y - (rgba.x + rgba.z) / 2.0f;
	float B = rgba.z - (rgba.x + rgba.y) / 2.0f;
	float Y = (rgba.x + rgba.y) / 2.0f - abs(rgba.x - rgba.y) / 2.0f - rgba.z;
	float L = (rgba.x + rgba.y + rgba.z) / 3.0f;
		/*abs(0.64f * r + 0.32 * g + 0.04 * b) / 1.0f;*/

	return make_float3(R - G, B - Y, L);
}

//__device__ float2 RGBY(optix::buffer<float4, 2> &buffer, const uint2 bufferA, const uint2 bufferB) {
//	float4 rgbaA = buffer[bufferA];
//	float4 rgbaB = buffer[bufferB];
//	float r_g = abs(abs(rgbaB.y - rgbaA.x) - abs(rgbaB.x - rgbaB.y)) / 3.0f;
//	float b_y = abs(abs(rgbaB.z - (rgbaA.y + rgbaA.x) / 2.0f) - abs((rgbaB.y + rgbaB.x) / 2.0f - rgbaA.z)) / 3.0f;
//
//	return make_float2(r_g, b_y);
//}

__device__ __inline__ float luminance(optix::buffer<float4, 2> &bufferA, const uint2 launch_indexA, const uint2 launch_indexB) {
	float4 rgbaA = bufferA[launch_indexA];
	float4 rgbaB = bufferA[launch_indexB];
	float3 coeff = make_float3(0.64f, 0.32f, 0.04f);
	return abs((coeff.x * rgbaA.x + coeff.y * rgbaA.y + coeff.z * rgbaA.z) - (coeff.x * rgbaB.x + coeff.y * rgbaB.y + coeff.z * rgbaB.z)) / 3.0f;
}
__device__ __inline__ float depth_saliency(optix::buffer<float4, 2> &buffer, const uint2 launch_index, const uint2 gaze, const float theta = 1.0f) {
	float focal_length = buffer[gaze].x;
	float depth = buffer[launch_index].x - focal_length;
	
	float depth_2 = depth * depth;
	float d = 0.4f * theta;
	float d_2 = d * d;
	float ad = 1.0f * theta;

	return 1.0f / (d * sqrt(2.0f * M_PIf)) * exp(-depth_2 / d_2) * ad;
}

__device__ __constant__ float gx[9] = {
	-1.0f, -0.0f, +1.0f,
	-2.0f, +0.0f, +2.0f,
	-1.0f, -0.0f, +1.0f
};
__device__ __constant__ float gy[9] = {
	-1.0f, -2.0f, -1.0f,
	-0.0f, +0.0f, +0.0f,
	+1.0f, +2.0f, +1.0f
};

__device__ float edge_detect(optix::buffer<float4, 2> &buffer, const uint2 launch_uv, const float2 bufferSize) {
	// 엣지 찾기
	float sumA = 0.0f, sumB = 0.0f, isBoundary = 0.0f;
	for (int i = 0; i < 9; i++) {
		uint2 edge_uv = launch_uv + offset[i];
		if (edge_uv.x < 0 || edge_uv.x >= bufferSize.x || edge_uv.y < 0 || edge_uv.y >= bufferSize.y)
			continue;

		float depth = buffer[edge_uv].x;
		sumA += depth * gx[i];
		sumB += depth * gy[i];
	}
	return isBoundary = clamp(((sumA > 0.0f ? sumA : -sumA) + (sumB > 0.0f ? sumB : -sumB)), 0.0f, 1.0f) > 0.99f;
}

__device__ __inline__ float gradient_x(optix::buffer<float4, 2> &buffer, const uint2 launch_uv, const float2 bufferSize, const int scale = 1) {
	float result = 0.0f;
	for (int i = 0; i < 9; i++) {
		uint2 kernel_uv = launch_uv + offset[i] * scale;
		if (kernel_uv.x < 0 || kernel_uv.x >= bufferSize.x || kernel_uv.y < 0 || kernel_uv.y >= bufferSize.y)
			continue;
		float4 data = buffer[kernel_uv];
		result += (data.x + data.y + data.z) / 3.0f * gx[i];
	}
	return result;
}
__device__ __inline__ float gradient_y(optix::buffer<float4, 2> &buffer, const uint2 launch_uv, const float2 bufferSize, const int scale = 1) {
	float result = 0.0f;
	for (int i = 0; i < 9; i++) {
		uint2 kernel_uv = launch_uv + offset[i] * scale;
		if (kernel_uv.x < 0 || kernel_uv.x >= bufferSize.x || kernel_uv.y < 0 || kernel_uv.y >= bufferSize.y)
			continue;
		float4 data = buffer[kernel_uv];
		result += (data.x + data.y + data.z) / 3.0f * gy[i];
	}
	return result;
}
__device__ __inline__ float orientation_by_sobel(const float gx, const float gy) {
	return atan(gy / gx);
}
__device__ float gradient(optix::buffer<float4, 2> &buffer, const uint2 launch_uv, const float2 bufferSize, const int scale = 1) {
	float gx = gradient_x(buffer, launch_uv, bufferSize, scale);
	float gy = gradient_y(buffer, launch_uv, bufferSize, scale);

	return sqrt(gx * gx + gy * gy);
}


__device__ float4 smooth_buffer_pixel(optix::buffer<float4, 2> &buffer, const uint2 index, const float2 bufferSize) {
	float4 pixel = make_float4(0.0f);
	int count = 0;
	for (int i = 0; i < 9; i++) {
		uint2 uv = index + offset[i];
		if (uv.x < 0 || uv.x >= bufferSize.x || uv.y < 0 || uv.y >= bufferSize.y)
			continue;

		pixel += buffer[uv];
		count++;
	}
	return pixel / (count < 1.0f ? 1.0f : count);
}

// position to screen uv
__device__ __inline__ float2 compute_reprojection(const optix::float3& p, optix::Matrix4x4& mvp, optix::float2& screen)
{
	float4 p_cs = make_float4(p, 1.0f);
	p_cs = mvp * p_cs;
	float2 d_cs = make_float2(p_cs) / p_cs.w;
	//float2 q_uv = (d_cs + 1.0f) * 0.5f * screen;
	float2 q_uv = (d_cs * screen + screen) * 0.5f;
	
	return q_uv;
}

// position to screen uv
__device__ __inline__ float2 compute_reprojection_pinhole(const float3& p, float3& cam_pos, float3& cam_target, float3 cam_up, float2& screen)
{
	float3 rendercamview = cam_target; rendercamview = normalize(rendercamview);
	float3 rendercamup = cam_up; rendercamup = normalize(rendercamup);
	float3 horizontalAxis = cross(rendercamview, rendercamup); horizontalAxis = normalize(horizontalAxis);
	float3 verticalAxis = cross(horizontalAxis, rendercamview); verticalAxis = normalize(verticalAxis);
	
	//reflect(p, horizontalAxis);

	float3 eye_to_p = normalize(p - cam_pos);
	float2 screen_uv = make_float2(dot(eye_to_p, horizontalAxis), dot(eye_to_p, verticalAxis)) / tan(45.0f * 0.5f * (M_PIf / 180.0f));

	return (screen_uv * screen + screen) * 0.5f;
}

__device__ __inline__ float velocity_map(const float velocity) {
	float m = -0.4f;
	float m_2 = m * m;
	float Am = 20.0f;
	float v_of_Am_2 = (velocity / Am)  * (velocity / Am);
	return 1.0f / (m * sqrt(2.0f * M_PIf)) * exp(-v_of_Am_2 / m_2) + 1.0f;
}

__device__ __inline__ float linearize_depth(float d, const float n, const float f) {
	float depthSample = 2.0f * d - 1.0f;
	float zLinear = 2.0f * n * f / (f + n - depthSample * (f - n));
	return zLinear;
}

// 인텐시티 시각화
__device__ float3 cool2warm(const float intensity) {
	float3 color;
	if (intensity <= 0.5f) {
		color.x = 0.0f; color.y = intensity * 2.0f; color.z = 1.0f - intensity * 2.0f;
	}
	else {
		color.x = (intensity - 0.5f) * 2.0f; color.y = (1.0f - intensity) * 2.0f; color.z = 0.0f;
	}
	return color;
}

__device__ float3 heatmap(const float intensity) {
	return make_float3(cos(intensity * M_PI_2f - M_PI_2f), sin(intensity * M_PIf) * 1.5f, cos(intensity * M_PI_2f));
}

/// 샘플링
#define MASK_SIZE 4
__device__ __constant__ bool mask_25[MASK_SIZE][MASK_SIZE] = {
	{ 1, 1, 0, 0 },
	{ 1, 1, 0, 0 },
	{ 1, 1, 1, 1 },
	{ 1, 1, 1, 1 },
};
__device__ __constant__ bool mask_50[MASK_SIZE][MASK_SIZE] = {
	{ 1, 1, 0, 0 },
	{ 1, 1, 0, 0 },
	{ 0, 0, 1, 1 },
	{ 0, 0, 1, 1 },
};
__device__ __constant__ bool mask_75[MASK_SIZE][MASK_SIZE] = {
	{ 1, 1, 0, 0 },
	{ 1, 1, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
};

__device__ __inline__ bool masked_sampling(const uint2 launch_index, const float sample_dist, const float sample_coef, float intensity) {
	bool isSample = false;
	float r0 = 0.07f;
	float r1 = r0 * 1.5f;
	float r2 = r0 * 2.0f;

	if (0 <= sample_dist && sample_dist < r0)
		isSample = 1;
	else if (r0 < sample_dist && sample_dist <= r1)
		isSample = mask_25[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (r1 < sample_dist && sample_dist <= r2)
		isSample = mask_50[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	/*else if (r2 < sample_dist)
		isSample = mask_75[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];*/

	int extra_sample_rate = 8;

	float g0 = 0.01f;
	float g1 = 0.4f;
	float g2 = 0.6f;
	float g3 = 0.8f;
	if (g0 < intensity && intensity < g1)
		isSample = isSample | mask_75[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (g1 <= intensity && intensity < g2)
		isSample = isSample | mask_50[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (g2 <= intensity)
		isSample = isSample | mask_25[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (g3 <= intensity)
		isSample = isSample | 1;
	else // 0
		isSample = isSample | ((launch_index.x % extra_sample_rate) == 0 && (launch_index.y % extra_sample_rate) == 0);

	/*if (launch_index.x < 512 && launch_index.y < 512)
		isSample = mask_25[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (512 <= launch_index.x && launch_index.y < 512)
		isSample = mask_50[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];
	else if (launch_index.x < 512 && 512 <= launch_index.y)
		isSample = 1;
	else
		isSample = mask_75[launch_index.x % MASK_SIZE][launch_index.y % MASK_SIZE];*/
		

	return isSample;
}

__device__ __inline__ void createPinHoleCam(const float3 cam_pos, const float3 cam_target, const float3 cam_up, const float3 gaze_target, const float2 pixel, float3& ray_orig, float3& ray_dir, const float aperture, uint seed0, uint seed1) {
	/* create a local coordinate frame for the camera */
	float3 rendercamview = cam_target; rendercamview = normalize(rendercamview);
	float3 rendercamup = cam_up; rendercamup = normalize(rendercamup);
	float3 horizontalAxis = cross(rendercamview, rendercamup); horizontalAxis = normalize(horizontalAxis);
	float3 verticalAxis = cross(horizontalAxis, rendercamview); verticalAxis = normalize(verticalAxis);

	float3 middle = cam_pos + rendercamview;
	float3 horizontal = horizontalAxis * tan(45.0f * 0.5f * (M_PIf / 180.0f));
	float3 vertical = verticalAxis * tan(45.0f * -0.5f * (M_PIf / 180.0f));

	float3 pointOnPlaneOneUnitAwayFromEye = middle + (horizontal * ((2 * pixel.x) - 1)) + (vertical * ((2 * pixel.y) - 1));
	float3 pointOnImagePlane = cam_pos + ((pointOnPlaneOneUnitAwayFromEye - cam_pos) * length(gaze_target - cam_pos)); /* cam->focalDistance */

	float3 aperturePoint = cam_pos;

	/* if aperture is non-zero (aperture is zero for pinhole camera), pick a random point on the aperture/lens */
	if (aperture - 0.07f > 0.00001f) {

		float random0 = get_random(seed0, seed1);
		float random1 = get_random(seed1, seed0);

		float angle = 2.0f * M_PIf * random0;
		float distance = aperture;// *sqrt(random1);
		float apertureX = cos(angle) * distance;
		float apertureY = sin(angle) * distance;

		aperturePoint = cam_pos + (horizontalAxis * apertureX) + (verticalAxis * apertureY);
	}

	float3 apertureToImagePlane = pointOnImagePlane - aperturePoint;
	apertureToImagePlane = normalize(apertureToImagePlane);

	/* create camera ray*/
	ray_orig = aperturePoint;
	ray_dir = apertureToImagePlane;
}

// 프레임 누적
__device__ __inline__ float4 color_to_accumulated(const float4& accum_color)
{
	float4 color = accum_color;
	if (color.w > 0.0f) {
		color.x /= accum_color.w;
		color.y /= accum_color.w;
		color.z /= accum_color.w;
		color.w = 1.0f;
	}
	return color;
}

// 톤맵핑
__device__ float3 uncharted2Tonemap(const float3 x) {
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

__device__ __inline__ float3 Uncharted2ToneMapping(const float3 color) {
	const float exposureBias = 2.0f;
	const float oo_gamma = 2.2f;
	float3 result = uncharted2Tonemap(exposureBias * color);
	float3 whiteScale = 1.0f / uncharted2Tonemap(make_float3(11.2f));
	result *= whiteScale;
	result = make_float3(pow(result.x, oo_gamma), pow(result.y, oo_gamma), pow(result.z, oo_gamma));

	return result;
}

// 로그폴라 변환
__device__ uint2 FowardLogPolar(uint2 xy, float2 center, float2 bufferSize) {
	uint2 uv = make_uint2(0.0f);
	float2 xy_prime = make_float2(xy) - center;

	float l1 = length(center);
	float l2 = length(bufferSize - center);
	float l3 = length(make_float2(center.x, bufferSize.y - center.y));
	float l4 = length(make_float2(bufferSize.x - center.x, center.y));
	float L = log(max(max(l1, l2), max(l3, l4)));

	uv.x = int(pow((log(length(xy_prime)) / L), 4.0f) * bufferSize.x);
	uv.y = int((atan2(xy_prime.y, xy_prime.x) + ((2.0f * M_PIf) * (xy_prime.y < 0.0f ? 1.0f : 0.0f))) * (bufferSize.y / (2.0f * M_PIf)));

	return uv;
}

__device__ uint2 InverseLogPolar(uint2 uv, float2 center, float2 bufferSize) {
	uint2 xy = make_uint2(-1.0f);

	if (uv.x < 0.0 || uv.x >= bufferSize.x || uv.y < 0.0 || uv.y >= bufferSize.y)
		return xy;

	float l1 = length(center);
	float l2 = length(bufferSize - center);
	float l3 = length(make_float2(center.x, bufferSize.y - center.y));
	float l4 = length(make_float2(bufferSize.x - center.x, center.y));
	float L = log(max(max(l1, l2), max(l3, l4)));

	float A = L;
	float B = (2.0f * M_PIf) / (bufferSize.y);

	float K = pow(uv.x / (bufferSize.x), 1.0f / 4.0f);
	xy.x = int(exp(L * K) * cos(B*uv.y) + center.x);
	xy.y = int(exp(L * K) * sin(B*uv.y) + center.y);

	return xy;
}