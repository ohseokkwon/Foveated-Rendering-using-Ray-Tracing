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
#include "device_include/random.h"

using namespace optix;

rtBuffer<uint3, 2>				 thread_buffer;
rtBuffer<uint3, 2>				 thread_cache;

rtDeclareVariable(uint2, sort_idx, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint, step, , );
rtDeclareVariable(uint, divide, , );
rtDeclareVariable(float2, gaze, , );

rtBuffer<uint, 1> ray_count;

//__device__ __constant__ uint2 offset[9] = {
//	(+1, -1), (+1, 0), (+1, +1),
//	(+0, -1), (+0, 0), (+0, +1),
//	(-1, -1), (-1, 0), (-1, +1)
//};

__device__ __constant__ uint2 offset[9] = {
	(+1, +0), (+1, +1),
	(+0, +0), (+0, +1),
};
//__device__ __constant__ uint2 offset[25] = {
//	(+2, -2), (+2, -1), (+2, +0), (+2, +1), (+2, +2),
//	(+1, -2), (+1, -1), (+1, +0), (+1, +1), (+1, +2),
//	(+0, -2), (+0, -1), (+0, +0), (+0, +1), (+0, +2),
//	(-1, -2), (-1, -1), (-1, +0), (-1, +1), (-1, +2),
//	(-2, -2), (-2, -1), (-2, +0), (-2, +1), (-2, +2),
//};


RT_PROGRAM void warp_sort()
{
	size_t2 screen = thread_buffer.size();
	uint2 self_coord, partner_coord;

	uint2 uv = make_uint2(0, launch_index.y);
	uint2 end_uv = make_uint2(screen.x - 1, launch_index.y);
	uint3 self, partner;

	if (step == 30) {
		ray_count[0] = 0;
		for (int i = 0; i < screen.y; i++)
			ray_count[0] += thread_cache[make_uint2(0, i)].z;

		return;
	}
	// CSR format comppress
	/*if (step == 0) {
		for (int i = 0; i < screen.x - 1; i++) {
			partner_coord = make_uint2(i, launch_index.y);
			partner = thread_buffer[partner_coord];

			if (partner.z > 0) {
				thread_cache[uv] = partner;
				thread_cache[make_uint2(0, launch_index.y)].z++;
				uv.x++;
			}
			else {
				thread_cache[end_uv] = make_uint3(partner_coord, 0);
				end_uv.x--;
			}
		}
	}
	else if (step == 1) {
		end_uv = make_uint2(screen.x - 1, launch_index.y);
		for (int i = 0; i < screen.x - 1; i++) {
			partner_coord = make_uint2(i, screen.x-1 - launch_index.y);
			partner = thread_cache[partner_coord];
			self = thread_cache[end_uv];

			if (self.z > 0)
				break;
			else if (partner.z > 0) {
				thread_cache[end_uv] = partner;
				thread_cache[partner_coord] = self;
				end_uv.x--;
			}
		}
	}
	else if (step == 2) {
		for (int i = 0; i < screen.y/2 - 1; i++) {
			partner_coord = make_uint2(launch_index.x, i + screen.y/2);
			self_coord = make_uint2(launch_index.x, i);

			partner = thread_cache[partner_coord];
			self = thread_cache[self_coord];

			if (self.z < 1 && partner.z > 0) {
				thread_cache[self_coord] = partner;
				thread_cache[partner_coord] = self;
			}
		}
	}*/

	//// CSR and last edge equalization
	if (step == 0) {
		uv = make_uint2(0, launch_index.y);
		end_uv = make_uint2(screen.x - 1, launch_index.y);

		for (int i = 0; i < screen.x; i++) {
			partner_coord = make_uint2(i, launch_index.y);
			partner = thread_buffer[partner_coord];

			if (partner.z > 0) {
				thread_cache[uv] = partner;
				thread_cache[make_uint2(0, launch_index.y)].z = uv.x+1;
				uv.x++;
			}
			else {
				thread_cache[end_uv] = partner;
				end_uv.x--;
			}
		}
	}
	else if (step == 31) {
		uv = make_uint2(launch_index.x, 0);
		end_uv = make_uint2(launch_index.x, screen.y - 1);

		for (int i = 0; i < screen.y; i++) {
			partner_coord = make_uint2(launch_index.x, i);
			partner = thread_cache[partner_coord];

			if (partner.z > 0) {
				thread_buffer[uv] = partner;
				thread_buffer[make_uint2(launch_index.x, 0)].z = uv.y + 1;
				uv.y++;
			}
			else {
				thread_buffer[end_uv] = partner;
				end_uv.y--;
			}
		}
	}
	else if (step > 0 && step < 10) {
		int start_offset_y = launch_index.y * divide;
		uint3 maxElement = thread_buffer[make_uint2(0, start_offset_y + 0)];
		uint3 minElement = thread_buffer[make_uint2(0, start_offset_y + 0)];
		uint2 maxIdx = make_uint2(0, start_offset_y), minIdx = make_uint2(0, start_offset_y);
		for (int i = 0; i < step * 2; i++) {
			if (maxElement.z < thread_buffer[make_uint2(0, start_offset_y + i)].z) {
				maxIdx = make_uint2(0, start_offset_y + i);
			}
			if (minElement.z > thread_buffer[make_uint2(0, start_offset_y + i)].z) {
				minIdx = make_uint2(0, start_offset_y + i);
			}
		}

		int max_length = thread_buffer[maxIdx].z, min_length = thread_buffer[minIdx].z;
		uint3 temp;
		uint2 max_target, min_target;
		
		while ((max_length - min_length) > 1) {
			// 타겟지정
			max_target = make_uint2(max_length-1, maxIdx.y);
			min_target = make_uint2(min_length+1, minIdx.y);

			// 색상 보기용
			thread_buffer[min_target].z = 2.0f;
			thread_buffer[max_target].z = 3.0f;

			// 양끝을 서로 교환
			temp = thread_buffer[max_target];
			thread_buffer[max_target] = thread_buffer[min_target];
			thread_buffer[min_target] = temp;

			// 교환후 서로 길이 조정
			thread_buffer[maxIdx].z--;
			thread_buffer[minIdx].z++;

			// 새 min/max 검출
			maxElement = thread_buffer[maxIdx];
			minElement = thread_buffer[minIdx];
			for (int i = 0; i < step * 2; i++) {
				if (maxElement.z < thread_buffer[make_uint2(0, start_offset_y + i)].z) {
					maxIdx = make_uint2(0, start_offset_y + i);
				}
				if (minElement.z > thread_buffer[make_uint2(0, start_offset_y + i)].z) {
					minIdx = make_uint2(0, start_offset_y + i);
				}
			}
			
			// 새로 찾은 min/max의 길이 찾기
			max_length = thread_buffer[maxIdx].z;
			min_length = thread_buffer[minIdx].z;
		}
	}
	//// Log-Polar
	//else if (step == 20) {
	//	float2 screenf = make_float2(screen);

	//	uint2 uv = FowardLogPolar(make_float2(launch_index), gaze, screenf);

	//	partner = thread_buffer[launch_index];
	//	if(partner.z > 0) {
	//		thread_cache[uv] = thread_buffer[launch_index];
	//	}
	//}
	//// Inverse Log-Polar
	//else if (step == 21) {
	//	for (int i = 0; i < screen.x; i++) {
	//		partner_coord = make_uint2(i, launch_index.y);
	//		partner = thread_cache[partner_coord];

	//		if (partner.z > 0) {
	//			thread_buffer[uv] = partner;
	//			thread_buffer[make_uint2(0, launch_index.y)].z = uv.x + 1;
	//			uv.x++;
	//		}
	//		else {
	//			thread_buffer[end_uv] = partner;
	//			end_uv.x--;
	//		}
	//	}
	//}

	// worst case
	//int endYcount = screen.x / 8;
	//int Ycount = 0;
	//uint2 package_warp = make_uint2(0, 0);
	//uint2 package_end = make_uint2(screen.x - 1, screen.y - 1);
	//for (int h = 0; h < screen.y; h++) {
	//	for (int w = 0; w < screen.x; w++) {
	//		thread_cache[make_uint2(w, h)] = make_uint4(0);
	//	}
	//}
	//for (int h = 0; h < screen.y; h++) {
	//	for (int w = 0; w < screen.x; w++) {
	//		uint4 thread_idx = thread_buffer[make_uint2(w, h)];
	//		
	//		if (thread_idx.z > 0) {
	//			thread_cache[package_warp] = thread_idx;
	//			thread_cache[package_warp].z = screen.x;
	//			package_warp.x++;
	//			if (package_warp.x % 8 == 0) {
	//				package_warp.x = Ycount*8;
	//				package_warp.y++;
	//				if (package_warp.y % screen.y == 0) {
	//					Ycount++;
	//					package_warp.x = Ycount * 8;
	//					package_warp.y = 0;
	//				}
	//			}
	//		}
	//		else {
	//			continue;
	//		}
	//	}
	//}

	// nearest block unit compress
	//int w = launch_index.x + blockDim.x;
	//self = thread_buffer[launch_index];
	//uv = make_uint2(0);
	//if (step % 2 == 0) {
	//	for (int y = 0; y < 2; y++) {
	//		for (int x = 0; x < 2; x++) {
	//			if (x == 0 && y == 0)
	//				continue;
	//			for (int i = 0; i < 4; i++) {
	//				// Get
	//				if ((launch_index.x / blockDim.x) % 2 == 0) {
	//					partner_coord = make_uint2(launch_index.x * 2 + x * blockDim.x,
	//						launch_index.y * 2 + y * blockDim.y) + offset[i];
	//					self_coord = launch_index * 2;

	//					if (partner_coord.x < 0.0 || partner_coord.x >= screen.y || partner_coord.y < 0.0 || partner_coord.y >= screen.y)
	//						continue;

	//					partner = thread_buffer[partner_coord];
	//					self = thread_buffer[self_coord];

	//					if (self.z > 1 && partner.z > 0) {
	//						thread_buffer[self_coord] = partner;
	//						thread_buffer[partner_coord] = self;
	//						break;
	//					}
	//				}
	//			}
	//		}
	//	}
	//}
	//else {
	//	for (int y = 0; y < 2; y++) {
	//		for (int x = 0; x < 2; x++) {
	//			if (x == 0 && y == 0)
	//				continue;
	//			for (int i = 0; i < 4; i++) {
	//				// Give
	//				if ((launch_index.x / blockDim.x) % 2 == 1) {
	//					partner_coord = make_uint2(launch_index.x * 2 + x * blockDim.x,
	//						launch_index.y * 2 + y * blockDim.y) + offset[i];
	//					self_coord = launch_index * 2;
	//					if (partner_coord.x < 0.0 || partner_coord.x >= screen.y || partner_coord.y < 0.0 || partner_coord.y >= screen.y)
	//						continue;

	//					partner = thread_buffer[partner_coord];
	//					self = thread_buffer[self_coord];

	//					if (self.z > 0 && partner.z < 1) {
	//						thread_buffer[partner_coord] = self;
	//						thread_buffer[self_coord] = partner;
	//						break;
	//					}
	//				}
	//			}
	//		}
	//	}
	//}

		/*for (int i = 0; i < 9; i++) {
			uv = make_uint2(w, launch_index.y) + offset[i];
			if (uv.x < 0.0 || uv.x >= screen.y || uv.y < 0.0 || uv.y >= screen.y)
				continue;

			partner = thread_buffer[uv];
			if (self.z < 1 && partner.z > 0) {
				thread_buffer[launch_index] = partner;
				thread_buffer[uv] = self;
				break;
			}
		}
	}
	else {
		for (int i = 0; i < 9; i++) {
			uv = make_uint2(w, launch_index.y) + offset[i];
			if (uv.x < 0.0 || uv.x >= screen.y || uv.y < 0.0 || uv.y >= screen.y)
				continue;

			partner = thread_buffer[uv];
			if (self.z > 0 && partner.z < 1) {
				thread_buffer[launch_index] = partner;
				thread_buffer[uv] = self;
				break;
			}
		}
	}*/

	/*uv = make_uint2(w, launch_index.y);
	if ((uv.x >= 0.0 && uv.x < screen.y && uv.y >= 0.0 && uv.y < screen.y)) {
		partner = thread_buffer[uv];
		if (step % 2 == 0) {
			if (self.z < 1 && partner.z > 0) {
				thread_buffer[launch_index] = partner;
				thread_buffer[uv] = self;
			}
		}
		else {
			if (self.z > 0 && partner.z < 1) {
				thread_buffer[launch_index] = partner;
				thread_buffer[uv] = self;
			}
		}
	}*/

	/*self = thread_buffer[launch_index];
	if(step == 0)
		partner_coord = make_uint2(launch_index.x, launch_index.y + screen.x * 0.5f);
	else if (step == 1)
		partner_coord = make_uint2(launch_index.x + screen.x * 0.5f, launch_index.y);
	else if (step == 2)
		partner_coord = make_uint2(launch_index.x + screen.x * 0.5f, launch_index.y + screen.x * 0.5f);

	for (int i = 0; i < 9; i++) {
		partner_coord += offset[i];
		if (partner_coord.x < 0.0 || partner_coord.x >= screen.y || partner_coord.y < 0.0 || partner_coord.y >= screen.y)
			continue;

		partner = thread_buffer[partner_coord];
		if (self.z < 1 && partner.z > 0) {
			thread_buffer[launch_index] = partner;
			thread_buffer[partner_coord] = self;
			break;
		}
	}*/
}