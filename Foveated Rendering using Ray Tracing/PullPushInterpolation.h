#pragma once

#include "gui.h"

class PullPushInterpolation
{
	Shader * m_PullShader = nullptr;
	Shader * m_PushShader = nullptr;
	Shader * m_FinalShader = nullptr;

	GLuint m_fbo_pyramid = 0;
	GLuint m_fbo = 0;
	GLuint m_buffer = GL_COLOR_ATTACHMENT0;

	glm::vec2 m_bufferSize = glm::vec2();
	GLuint m_pull_Tex = 0;
	GLuint m_push_Tex = 0;

	GLuint m_outputTex = 0;

public:
	PullPushInterpolation();
	~PullPushInterpolation();
	void render(const GLuint sparse, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);
	void resetShader();

private:
	void genFBO();

public:
	const GLuint& pull_Tex = m_pull_Tex;
	const GLuint& push_Tex = m_push_Tex;

	const GLuint& outputTex = m_outputTex;
};