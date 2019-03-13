#pragma once

#include "gui.h"

class LogPolarTransform {
	Shader * m_LPForwardShader = nullptr;
	Shader * m_LPInverseShader = nullptr;
	GLuint m_fbo = 0;
	GLuint m_tex[2] = {};

	glm::ivec2 m_LPbufferSize = glm::ivec2(0.0f);
	glm::vec2 m_LPScale = glm::vec2(1.0f);

	const GLenum m_buf[2] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1
	};

public:
	LogPolarTransform();
	~LogPolarTransform();
	void render(GLuint pt, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);
	void resetShader();

private:
	void genFBO();


public:
	const GLuint& logPolarTex = m_tex[0];
	const GLuint& ilogPolarTex = m_tex[1];
};