#version 430 core

layout(binding = 0) uniform sampler2D positionTex;
layout(binding = 1) uniform sampler2D normalTex;

layout(location = 0) uniform sampler2D colorTex;

layout(location = 1) uniform float c_phi; 
layout(location = 2) uniform float n_phi; 
layout(location = 3) uniform float p_phi; 
layout(location = 4) uniform float stepWidth;
layout(location = 5) uniform vec2 screenSize;
layout(location = 6) uniform vec2 eyeCoord; // 마우스 좌표 
layout(location = 7) uniform float apectureSize; // 동공의 반지름

layout(location = 0) out vec4 outColor;

const float kernel[25] = float[25](
	1.f/256.f,	1.f/64.f,	3.f/128.f,	1.f/64.f,	1.f/256.f,
	1.f/64.f,	1.f/16.f,	3.f/32.f,	1.f/16.f,	1.f/64.f,
	3.f/128.f,	3.f/32.f,	9.f/64.f,	3.f/32.f,	3.f/128.f,
	1.f/64.f,	1.f/16.f,	3.f/32.f,	1.f/16.f,	1.f/64.f,
	1.f/256.f,	1.f/64.f,	3.f/128.f,	1.f/64.f,	1.f/256.f
);
	// gaussian kernel
	//1.0f/273.0f, 4.0f/273.0f, 7.0f/273.0f, 4.0f/273.0f, 1.0f/273.0f,
	//4.0f/273.0f, 16.0f/273.0f, 26.0f/273.0f, 16.0f/273.0f, 4.0f/273.0f,
	//7.0f/273.0f, 26.0f/273.0f, 41.0f/273.0f, 26.0f/273.0f, 7.0f/273.0f,
	//4.0f/273.0f, 16.0f/273.0f, 26.0f/273.0f, 16.0f/273.0f, 4.0f/273.0f,
	//1.0f/273.0f, 4.0f/273.0f, 7.0f/273.0f, 4.0f/273.0f, 1.0f/273.0f

const vec2 offset[25] = vec2[25](
	vec2(-2, +2), vec2(-1, +2), vec2(0, +2), vec2(1, +2), vec2(2, +2),
	vec2(-2, +1), vec2(-1, +1), vec2(0, +1), vec2(1, +1), vec2(2, +1),
	vec2(-2, +0), vec2(-1, +0), vec2(0, +0), vec2(1, +0), vec2(2, +0),
	vec2(-2, -1), vec2(-1, -1), vec2(0, -1), vec2(1, -1), vec2(2, -1),
	vec2(-2, -2), vec2(-1, -2), vec2(0, -2), vec2(1, -2), vec2(2, -2)
);

void main(void) {
	vec2 FragCoord = gl_FragCoord.st / screenSize;

	// resolution
    vec4 pval = texture2D(positionTex, FragCoord);
    vec4 nval = texture2D(normalTex, FragCoord);
	vec4 cval = texture2D(colorTex, FragCoord);
	
    vec4 sum = vec4(0.0);
	float cum_w = 0.0;

	//float distance2 = distance(eyeCoord, gl_FragCoord.st) / length(screenSize);
	//float p_phi2 = 0.0f;
	//if(distance2 < apectureSize) {
	//	p_phi2 = p_phi;
	//} else {
	//	p_phi2 = mix(p_phi, p_phi*1000, distance2-apectureSize);
	//}

	for(int i = 0; i < 25; i++) {
        vec2 uv = gl_FragCoord.st + offset[i] * stepWidth;
		vec2 uv_low = gl_FragCoord.st + offset[i] * stepWidth;
		if(uv.s < 0.0 || uv.s >= screenSize.s || uv.t < 0.0 || uv.t >= screenSize.t)
			continue;
		
		// color
	    vec4 ctmp = texture2D(colorTex, uv / screenSize);
        vec4 t = cval - ctmp;
        float dist2 = dot(t,t);
        float c_w = min(exp(-(dist2)/c_phi), 1.0);
        
		// normal
		vec4 ntmp = texture2D(normalTex, uv_low / screenSize);
		t = nval - ntmp;
        dist2 = max(dot(t,t)/(stepWidth*stepWidth),0.0);
		float n_w = min(exp(-(dist2)/n_phi), 1.0);
        
		// position
		vec4 ptmp = texture2D(positionTex, uv_low / screenSize);
		t = pval - ptmp;
        dist2 = dot(t,t);
		float p_w = min(exp(-(dist2)/p_phi),1.0);
        
		float weight = c_w * n_w * p_w;
		sum += ctmp * weight * kernel[i];
        cum_w += weight*kernel[i];
    }

	outColor = (sum/cum_w);
	//outColor = cval;
}
