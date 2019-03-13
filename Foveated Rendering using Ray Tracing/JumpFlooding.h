#pragma once

#include "gui.h"

class JumpFlooding
{
	Shader * m_JFShader = nullptr;
	Shader * m_CPShader = nullptr;
	GLuint m_fbo = 0;
	GLuint m_pbo[2] = {0};
	GLuint m_tex[4] = {}; // coordA, colorA, coordB, colorB
	GLenum m_buf[2][2] = {
		{ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 },
		{ GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 } };
	int m_maxStep = 1;
	GLuint m_coordTex = 0;
	GLuint m_colorTex = 0;

public:
	JumpFlooding();
	~JumpFlooding();
	void render(const GLuint rt, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);
	void resetShader();

private:
	void genFBO();
	void genPBO();

public:
	const GLuint& coordTex = m_coordTex;
	const GLuint& colorTex = m_colorTex;
};