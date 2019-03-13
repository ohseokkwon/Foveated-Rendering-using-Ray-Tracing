#version 430 core

layout(binding = 0) uniform sampler2DRect depthTex;
layout(binding = 1) uniform sampler2DRect coordTex;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outIntensity;
layout(location = 2) out vec4 outOrientation;

layout(location = 0) uniform vec2 screenSize;

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
const float kernel[9] = float[9](
	-1.0f, -1.0f, -1.0f,
	-1.0f, +9.0f, -1.0f,
	-1.0f, -1.0f, -1.0f
);
const vec2 offset[9] = vec2[9](
	vec2(-1, +1), vec2(0, +1), vec2(+1, +1),
	vec2(-1, +0), vec2(0, +0), vec2(+1, +0),
	vec2(-1, -1), vec2(0, -1), vec2(+1, -1)
);

void main()
{
	float g_buffer_scale = 0.5f;
    float sumA = (0.0f);
	float sumB = (0.0f);
	vec3 edge = vec3(0.0f);

	for(int i=0; i<9; i++) {
		vec2 uv = texture(coordTex, (gl_FragCoord.st * g_buffer_scale)).xy + offset[i];
		if(uv.s < 0.0 || uv.s >= screenSize.s || uv.t < 0.0 || uv.t >= screenSize.t)
			continue;
		
		float value = texture(depthTex, uv).a;
		sumA += value * gx[i];
		sumB += value * gy[i];
		//edge += value * kernel[i];
	}
	
	//outColor = vec4(clamp(abs(color.r)-1.0f, 0.0f, 1.0f));
	float outValue = (sumA > 0.0f ? sumA : -sumA) + (sumB > 0.0f ? sumB : -sumB);
	outColor = vec4(outValue);
	
	//outColor = vec4(edge, 1.0f);

	//outColor = texture(depthTex, gl_FragCoord.st * 0.5f);
}
