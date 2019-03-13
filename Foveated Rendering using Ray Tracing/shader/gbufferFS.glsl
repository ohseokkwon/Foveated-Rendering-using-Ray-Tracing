#version 430 core

//layout(binding = 0) uniform sampler2D positionTex;
//layout(binding = 1) uniform sampler2D normalTex;
//layout(binding = 2) uniform sampler2D depthTex;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDepth;

in vec2 TexCoord;
in vec4 Position;
in vec3 Normal;
in float Depth;

layout(location = 3) uniform vec2 screenSize;

layout(std140, binding = 1) uniform Camera { // 144
	vec3 origin;   // 0   //12
	mat4 pmat;      // 16   //64
	mat4 vmat;      // 80   //64
};

void main()
{
	outPosition = vec4(Normal, 1.0f);
	outNormal = vec4(Normal, 1.0f);
	outDepth = vec4(Depth, Depth, Depth, 1.0f);
}
