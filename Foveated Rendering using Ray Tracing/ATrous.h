#pragma once

#include "gui.h"

class ATrous
{
	Shader * m_ATShader = nullptr;
	GLuint m_fbo = 0;
	GLuint m_tex[2] = {};
	const GLenum m_buf[2][1] = {
		{ GL_COLOR_ATTACHMENT0 },
		{ GL_COLOR_ATTACHMENT1 },
	};
	GLuint m_colorTex;

public:
	ATrous();
	~ATrous();
	void render(int count, GLuint positionTex, GLuint normalTex, GLuint colorTex, GLuint rtTex, const int frame,
		const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);

private:
	void genFBO();

public:
	const GLuint& colorTex = m_colorTex;
};