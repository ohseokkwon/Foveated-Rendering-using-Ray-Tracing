#version 430 core

layout(location = 0) uniform sampler2D inTex;

layout(location = 1) uniform vec2 screenSize;
layout(location = 2) uniform vec2 bufferSize;
layout(location = 3) uniform vec2 scale;
layout(location = 4) uniform vec2 gaze;

layout(binding = 3, rgba32f) coherent uniform image2D destTex;

#define Epsilon 0.00003f
#define PI		3.141592f
#define E		2.718281f

ivec2 InverseLogPolar(ivec2 uv, vec2 center) {
	ivec2 xy = ivec2(-1);

	if (uv.x < 0.0 || uv.x >= bufferSize.x*2.0f || uv.y < 0.0 || uv.y >= bufferSize.y*2.0f)
		return xy;

	float l1 = length(center);
	float l2 = length(screenSize - center);
	float l3 = length(vec2(center.x, screenSize.y - center.y));
	float l4 = length(vec2(screenSize.x - center.x, center.y));
	float L = log(max(max(l1, l2), max(l3, l4)));

	float A = L;
	float B = (2.0f * PI) / (bufferSize.y);

	float K = pow(uv.x / (bufferSize.x), 1.0f/4.0f);
	xy.x = int(exp(L * K) * cos(B*uv.y) + center.x);
	xy.y = int(exp(L * K) * sin(B*uv.y) + center.y);

	return xy;
}

ivec2 FowardLogPolar(vec2 xy, vec2 center) {
	ivec2 uv = ivec2(0);
	vec2 xy_prime = xy - center;

	float l1 = length(center);
	float l2 = length(screenSize - center);
	float l3 = length(vec2(center.x, screenSize.y - center.y));
	float l4 = length(vec2(screenSize.x - center.x, center.y));
	float L = log(max(max(l1, l2), max(l3, l4)));

	uv.x = int(pow((log(length(xy_prime)) / L), 4.0f) * bufferSize.x);
	uv.y = int((atan(xy_prime.y, xy_prime.x) + ((2.0f * PI) * (xy_prime.y < 0.0f ? 1.0f : 0.0f))) * (bufferSize.y / (2.0f * PI)));
	
	return uv;
}

const float gx[9] = float[9](
	-1.0f, -0.0f, +1.0f,
	-2.0f, +0.0f, +2.0f,
	-1.0f, -0.0f, +1.0f
	);
const float gy[9] = float[9](
	-1.0f, -2.0f, -1.0f,
	-0.0f, +0.0f, +0.0f,
	+1.0f, +2.0f, +1.0f
	);
const float weight[9] = float[9](
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f,
	1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 8.0f,
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f
	);
const vec2 offset[9] = vec2[9](
	vec2(-1, +1), vec2(0, +1), vec2(+1, +1),
	vec2(-1, +0), vec2(0, +0), vec2(+1, +0),
	vec2(-1, -1), vec2(0, -1), vec2(+1, -1)
	);

layout(local_size_x = 32, local_size_y = 32) in;
void main()
{
	vec2 FragCoord = gl_GlobalInvocationID.xy / screenSize;

	ivec2 uv = FowardLogPolar(gl_GlobalInvocationID.xy, gaze);
	ivec2 xy = InverseLogPolar(uv, gaze);
	vec4 data = texture2D(inTex, xy / screenSize);
	
	//// log-polar coordinate에서 엣지추출
	//float sumA = (0.0f);
	//float sumB = (0.0f);
	//for (int i = 0; i < 9; i++) {
	//	data += texture2D(inTex, (xy + offset[i]) / screenSize ) * weight[i];
	//	float value = texture2D(inTex, (xy + offset[i]) / screenSize).r;
	//	sumA += value * gx[i];
	//	sumB += value * gy[i];
	//}
	//float outValue = (sumA > 0.0f ? sumA : -sumA) + (sumB > 0.0f ? sumB : -sumB);
	//data = vec4(outValue);

	memoryBarrier();
	imageStore(destTex, uv, data);
}
