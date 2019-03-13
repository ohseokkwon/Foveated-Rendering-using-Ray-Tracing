#version 430 core

layout(location = 0) uniform sampler2D inTex;

layout(location = 1) uniform vec2 screenSize;
layout(location = 2) uniform vec2 bufferSize;
layout(location = 3) uniform vec2 searchOffset;
layout(location = 4) uniform vec2 writeOffset;
layout(location = 5) uniform int step;
layout(location = 6) uniform int count;

layout(binding = 0, rgba32f) coherent uniform image2D destTex;

#define Epsilon 0.00003f
#define PI_2 6.28319f

const vec2 offset[4] = vec2[4](
	vec2(+0, +0), vec2(+1, +0),
	vec2(+1, +1), vec2(+0, +1)
	);
const float pull_filter[4] = float[4](
	1.0f / 4.0f, 1.0f / 4.0f, 1.0f / 4.0f, 1.0f / 4.0f
	);

layout(local_size_x = 32, local_size_y = 32) in;
void main()
{
	vec2 FragCoord = (gl_GlobalInvocationID.xy - writeOffset.xy) / vec2(pow(2.0f, step));
	vec4 writeCoord = vec4(writeOffset, writeOffset + vec2(pow(2.0f, step)));

	vec4 next_data = textureLod(inTex, FragCoord, count);

	if (count < 0) {
		next_data = textureLod(inTex, FragCoord, 0);
	}

	ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
	vec4 write_buffer = imageLoad(destTex, xy);

	memoryBarrier();

	/*gID가 쓰기영역을 벗어나면*/
	if (gl_GlobalInvocationID.x < writeCoord.x || gl_GlobalInvocationID.x >= writeCoord.z ||
		gl_GlobalInvocationID.y < writeCoord.y || gl_GlobalInvocationID.y >= writeCoord.w) {
		imageStore(destTex, xy, write_buffer);
	}
	else {
		if (count > -1) {
			vec2 q_xy = xy;
			if (count < 1) {
				q_xy -= vec2(screenSize.x, screenSize.y * 0.5f-1); 
				q_xy *= 2.0f;
			}
			else {
				q_xy -= vec2(screenSize.x, pow(2.0f, step)-1);
				q_xy *= 2.0f;
				q_xy += vec2(screenSize.x, pow(2.0f, step)-1 + pow(2.0f, step));
			}

			int hitCount = 0;
			vec4 filtered_color = vec4(0.0f);
			for (int i = 0; i<4; i++) {
				vec2 kernel_coord = vec2(q_xy + offset[i]);

				vec4 ref_weight = imageLoad(destTex, ivec2(kernel_coord));
				if (ref_weight.a > 0) {
					filtered_color += ref_weight;// pull_filter[i];
					hitCount++;
				}
			}
			if (hitCount > 0)
				filtered_color /= filtered_color.a;
			filtered_color = vec4(filtered_color.rgb, (hitCount > 0 ? 1.0f : 0.0f));
			imageStore(destTex, xy, filtered_color);
		}
		else {
			imageStore(destTex, xy, next_data);
		}
	}
}
