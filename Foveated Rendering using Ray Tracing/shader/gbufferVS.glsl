#version 430 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vec2 TexCoord;
out vec4 Position;
out vec3 Normal;
out float Depth;

layout(std140, binding = 1) uniform Camera { // 144
	vec3 origin;   // 0   //12
	mat4 pmat;      // 16   //64
	mat4 vmat;      // 80   //64
};

void main()
{
	mat4 mvp = (pmat * vmat);
    gl_Position = mvp * vec4(position, 1.0f);
    TexCoord = texCoord;
	Position = gl_Position / gl_Position.w;
	Normal = normalize(normal) * 0.5f + 0.5f;
	Depth = length(gl_Position.xyz - origin);
}