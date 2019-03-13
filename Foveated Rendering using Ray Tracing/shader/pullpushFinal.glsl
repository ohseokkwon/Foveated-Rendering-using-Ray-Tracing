#version 430 core

//layout(location = 0) uniform image2D pullTex;

layout(location = 1) uniform vec2 screenSize;

layout(binding = 1, rgba32f) coherent uniform image2D pushTex;
layout(binding = 2, rgba32f) coherent uniform image2D destTex;

#define Epsilon 0.000003f
#define PI_2 6.28319f

layout(local_size_x = 32, local_size_y = 32) in;
void main()
{
	ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
	vec4 filled_data = imageLoad(pushTex, xy);
	imageStore(destTex, xy, filled_data);
}
