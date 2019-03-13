#version 430 core

//layout(location = 0) uniform image2D pullTex;

layout(location = 1) uniform vec2 screenSize;
layout(location = 2) uniform vec2 bufferSize;
layout(location = 3) uniform vec2 searchOffset;
layout(location = 4) uniform vec2 writeOffset;
layout(location = 5) uniform int step;
layout(location = 6) uniform int count;

layout(binding = 0, rgba32f) coherent uniform image2D pullTex;
layout(binding = 1, rgba32f) coherent uniform image2D destTex;

#define Epsilon 0.000003f
#define PI_2 6.28319f

//const vec2 offset[4] = vec2[4](
//	vec2(+0, +0), vec2(+1, +0),
//	vec2(+1, +1), vec2(+0, +1)
//	);
//const float push_filter[4] = float[4](
//	9.0f / 16.0f, 3.0f / 16.0f, 3.0f / 16.0f, 1.0f / 16.0f
//	);
const vec2 offset[9] = vec2[9](
	vec2(+1, -1), vec2(+1, +0), vec2(+1, +1),
	vec2(+0, -1), vec2(+0, +0), vec2(+0, +1),
	vec2(-1, -1), vec2(-1, +0), vec2(-1, +1)
	);

const float push_filter[9] = float[9](
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f,
	1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 8.0f,
	1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 16.0f
	);
#define FILTER_SIZE 9

layout(local_size_x = 32, local_size_y = 32) in;
void main()
{
	vec2 FragCoord = (gl_GlobalInvocationID.xy - writeOffset.xy) / vec2(pow(2.0f, count));
	vec4 writeCoord = vec4(writeOffset, writeOffset + vec2(pow(2.0f, count)));

	if (count < 0) {
		writeCoord = vec4(writeOffset, writeOffset + vec2(pow(2.0f, 0)));
	}

	ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
	vec4 write_buffer = imageLoad(destTex, xy);

	vec4 next_data = imageLoad(pullTex, xy);

	memoryBarrier();

	/*gID가 쓰기영역을 벗어나면*/
	if (gl_GlobalInvocationID.x < writeCoord.x || gl_GlobalInvocationID.x >= writeCoord.z ||
		gl_GlobalInvocationID.y < writeCoord.y || gl_GlobalInvocationID.y >= writeCoord.w) {
		imageStore(destTex, xy, write_buffer);
	}
	else {
		ivec2 q_xy = xy;

		if (step > 0) {
			q_xy -= ivec2(screenSize.x, pow(2.0f, count) - 1);
			q_xy /= 2;
			q_xy += ivec2(screenSize.x, pow(2.0f, count) - 1 - pow(2.0f, count - 1));
		}
		else {
			q_xy /= 2;
			q_xy += ivec2(screenSize.x, screenSize.y * 0.5f - 1);
		}

		if (count > 0) {
			if (next_data.a > 0) {
				imageStore(destTex, xy, next_data);
			}
			else {
				vec4 find_color = vec4(0.0f);
				int find_idx = 0;
				for (int i = 0; i < FILTER_SIZE; i++) {
					vec2 kernel_coord = vec2(q_xy + offset[i]);
					find_color = imageLoad(pullTex, ivec2(kernel_coord));
					if (find_color.a > 0) {
						find_idx = i;
						break;
					}
				}

				vec4 filtered_color = vec4(0.0f);
				for (int i = 0; i < FILTER_SIZE; i++) {
					vec2 kernel_coord = vec2(q_xy + offset[(i+ find_idx)%FILTER_SIZE]);
					filtered_color += push_filter[i] * imageLoad(destTex, ivec2(kernel_coord));
				}
				imageStore(destTex, xy, filtered_color);
			}
		}
		else {
			next_data = imageLoad(pullTex, ivec2(xy));
			imageStore(destTex, xy, next_data);
		}
	}
}
