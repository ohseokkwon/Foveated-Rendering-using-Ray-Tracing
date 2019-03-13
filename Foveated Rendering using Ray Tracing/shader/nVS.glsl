#version 430 core

const vec2 positions[4] = vec2[4](
	vec2(-1.f, -1.f),
	vec2(+1.f, -1.f),
	vec2(+1.f, +1.f),
	vec2(-1.f, +1.f)
);


void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.f, 1.f);
}
