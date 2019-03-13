#version 430 core

layout(location = 0) uniform sampler2D coordTex;
layout(location = 1) uniform sampler2D colorTex;

layout(location = 2) uniform vec2 screenSize;
layout(location = 3) uniform float step_;

layout(location = 0) out vec4 outCoord;
layout(location = 1) out vec4 outColor;

void main()
{
	vec2 FragCoord = gl_FragCoord.st / screenSize;

	// 8 방위의 정점
	vec2 nCoord[8];
	nCoord[0] = vec2(gl_FragCoord.s - step_	, gl_FragCoord.t - step_);
	nCoord[1] = vec2(gl_FragCoord.s			, gl_FragCoord.t - step_);
	nCoord[2] = vec2(gl_FragCoord.s + step_	, gl_FragCoord.t - step_);
	nCoord[3] = vec2(gl_FragCoord.s - step_	, gl_FragCoord.t        );
	nCoord[4] = vec2(gl_FragCoord.s + step_	, gl_FragCoord.t		);
	nCoord[5] = vec2(gl_FragCoord.s - step_	, gl_FragCoord.t + step_);
	nCoord[6] = vec2(gl_FragCoord.s			, gl_FragCoord.t + step_);
	nCoord[7] = vec2(gl_FragCoord.s + step_	, gl_FragCoord.t + step_);

	// 기록된 Coordination과 Color
	vec4 coord = texture2D(coordTex, FragCoord.st);
	vec4 color = texture2D(colorTex, FragCoord.st);

	// 현재 색을 위해 참고 하고 있는 위치와의 거리를 구한다.
	float dist = 0.f;
	if( coord.a > 0.0f )
		dist = distance(coord.st, FragCoord.st);

	// 8개의 uv 차로 이동해서, 거리가 더 짧아지면 채택한다.(dist로 삽입한다)
	for(int i = 0; i < 8; ++i)
	{
		vec2 q_uv = nCoord[i] / screenSize;
		// UV가 화면 범위를 벗어나면 스?
		if( q_uv.s < 0.0 || q_uv.s >= 1.0f || q_uv.t < 0.0 || q_uv.t >= 1.0f)
			continue;

		vec4 neighbor = texture2D(coordTex, q_uv);
		if( neighbor.a < 1.0f )
			continue;

		float newDist = distance(neighbor.st, FragCoord.st);

		if( coord.a < 1.0f || newDist < dist ) {
			coord = neighbor;
			color = texture2D(colorTex, q_uv);
			dist = newDist;
		}
	}

	outCoord = coord;
	outColor = color;
	//outColor = texture2DRect(colorTex, FragCoord.st);

	//if((FragCoord.s <= 0.5f) && (FragCoord.t <= 0.5f)) {
	//	outCoord = vec4(0.25f, 0.25f, 0, 1.0f);
	//	outColor = vec4(0, 0, 1, 1.0f);
	//}
	//else if((0.5f < FragCoord.s) && (FragCoord.t <= 0.5f)) {
	//	outCoord = vec4(0.75f, 0.25f, 0, 1.0f);
	//	outColor = vec4(0, 1, 0, 1.0f);
	//}
	//else if((FragCoord.s <= 0.5f) && (0.5f < FragCoord.t)) {
	//	outCoord = vec4(0.25f, 0.75f, 0, 1.0f);
	//	outColor = vec4(1, 0, 0, 1.0f);
	//}
	//else if((0.5f < FragCoord.s) && (FragCoord.t < 1.0f)) {
	//	outCoord = vec4(0.75f, 0.75f, 0, 1.0f);
	//	outColor = vec4(1, 1, 0, 1.0f);
	//}

	//if(FragCoord.s <= 0.5f) {
	//	outCoord = vec4(0.25f, 0.5f, 0, 1.0f);
	//	outColor = vec4(1, 0, 0, 1.0f);
	//}
	//else if(0.5f < FragCoord.s) {
	//	outCoord = vec4(0.75f, 0.5f, 0, 1.0f);
	//	outColor = vec4(0, 0, 1, 1.0f);
	//}
		
}