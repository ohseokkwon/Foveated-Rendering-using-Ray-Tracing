#version 430 core

layout(binding = 0) uniform sampler2D coordTex;
layout(binding = 1) uniform sampler2D colorTex;

layout(location = 0) out vec4 outColor;

layout(location = 1) uniform vec2 screenSize;

#define Epsilon 0.00003f
#define PI_2	6.28319f
#define PI		3.141592f

// my mehtod (very fast)
#if 1
void main()
{
	vec2 FragCoord = gl_FragCoord.st / screenSize;

	vec4 closest = texture2D(coordTex, FragCoord.st);
	vec4 closestColor = texture2D(colorTex, closest.st);

	float dist_to_closest = distance(closest.st, FragCoord.st);

	vec4 incrementColor = vec4(0.0f);
	vec2 min_box = FragCoord - dist_to_closest;
	vec2 max_box = FragCoord + dist_to_closest;
	vec2 increment = vec2(1.0f) / screenSize;

	for(float h=min_box.y; h<max_box.y; h+=increment.y) {
        for(float w=min_box.x; w<max_box.x; w+=increment.x) {
            vec2 reference = vec2(w, h);
			if(reference.s < 0.0 || reference.s >= 1.0f|| reference.t < 0.0 || reference.t >= 1.0f)
				continue;

			float radius = distance(FragCoord.st, reference.st);
			if(radius > dist_to_closest)
				continue;

			vec4 ref_closestColor = texture2D(colorTex, reference.st);
			incrementColor += vec4(ref_closestColor.xyz, 1);
		}
	}

	if(incrementColor.a > 0.0f)
		outColor = vec4(incrementColor.rgb / incrementColor.a, 1.0f);
	else
		outColor = closestColor;
}
#else
// Park et al. mehtod (fast but slow)
void main()
{
	vec2 FragCoord = gl_FragCoord.st / screenSize;

	vec4 closest = texture2D(coordTex, FragCoord.st);
	vec4 closestColor = texture2D(colorTex, closest.st);

	float min_probability = 0.005f;
	vec4 incrementColor = vec4(0.0f);
	vec2 min_box = FragCoord - min_probability;
	vec2 max_box = FragCoord + min_probability;
	vec2 increment = vec2(1.0f) / screenSize;

	for (float h = min_box.y; h < max_box.y; h += increment.y) {
		for (float w = min_box.x; w < max_box.x; w += increment.x) {
			vec2 reference = vec2(w, h);
			if (reference.s < 0.0 || reference.s >= 1.0f || reference.t < 0.0 || reference.t >= 1.0f)
				continue;

			closest = texture2D(coordTex, reference.st);
			float radius = distance(closest.st, reference.st);
			float dist_i_to_p = distance(FragCoord.st, reference.st);
			if (radius < dist_i_to_p)
				continue;

			vec4 ref_closest = texture2D(coordTex, reference);
			vec4 ref_closestColor = texture2D(colorTex, reference.st);
			incrementColor += vec4(ref_closestColor.xyz, 1.0f);
		}
	}

	if (incrementColor.a > 0.0f)
		outColor = vec4(incrementColor.rgb / incrementColor.a, 1.0f);
	else
		outColor = closestColor;
}
#endif
