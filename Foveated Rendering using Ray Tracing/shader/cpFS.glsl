#version 430 core

layout(binding = 0) uniform sampler2D colorTex;

layout(location = 0) out vec4 outCoord;
layout(location = 1) out vec4 outColor;

layout(location = 0) uniform vec2 screenSize;

void main()
{
	vec2 FragCoord = gl_FragCoord.st / screenSize;

    vec4 color = texture(colorTex, FragCoord);

	outCoord = vec4(FragCoord, 0, color.a);
	outColor = color;
}
