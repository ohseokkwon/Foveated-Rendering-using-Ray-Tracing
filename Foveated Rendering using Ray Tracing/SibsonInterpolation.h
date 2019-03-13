#pragma once

#include "gui.h"

class SibsonInterpolation
{
	Shader * m_SIShader = nullptr;

	GLuint m_fbo = 0;
	GLenum m_buffers[1] = {
		 GL_COLOR_ATTACHMENT0  };

	GLuint m_outputTex = 0;

public:
	SibsonInterpolation();
	~SibsonInterpolation();
	void render(const GLuint coord, const GLuint color, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);
	void resetShader();

private:
	void genFBO();

public:
	const GLuint& outputTex = m_outputTex;
};