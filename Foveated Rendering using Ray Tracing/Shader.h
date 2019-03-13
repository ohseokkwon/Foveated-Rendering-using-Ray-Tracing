#pragma once

#include <GL/glew.h>

class Shader
{
	GLuint m_id;

public:
	~Shader();
	
	Shader(const char* vertFile, 
		const char* fragFile,
		const bool isCompute = 0);
	
	Shader(const char* vertFile,
		const char* geomFile,
		const char* fragFile);

	Shader(const char* vertFile, 
		const char* tessContFile, 
		const char* tessEvalFile,
		const char* fragFile);

	Shader(const char* vertFile,
		const char* tessContFile,
		const char* tessEvalFile,
		const char* geomFile,
		const char* fragFile);

	GLuint getProgramID() const;
	void use() const;
	void unuse() const;

private:
	static GLuint genShader(GLenum type, const char *file);
	static GLuint genProgram(const char* vertFile, 
		const char* tessContFile,
		const char* tessEvalFile,
		const char* geomFile, 
		const char* fragFile);

	static GLuint genProgram(const char* computeFragFile);
};