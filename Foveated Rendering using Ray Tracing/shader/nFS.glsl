#version 430 core
#extension GL_NV_shader_thread_group : require

layout(binding = 0) uniform sampler2D someTex;

layout(location = 0) uniform vec2 screenSize;

out vec4 fragColor;

void main()
{
	vec2 FragCoord = gl_FragCoord.st / screenSize;

	fragColor = texture2D(someTex, FragCoord);

/*
	vec3 color;
	if(gl_WarpIDNV == 0) 
	{
		color.g = float(gl_ThreadInWarpNV) / float(gl_WarpSizeNV-1);
	}
	else
	{
		color.r = float(gl_SMIDNV) / float(gl_SMCountNV-1);
	}
	fragColor = vec4(color, 1.0f);
*/
}
