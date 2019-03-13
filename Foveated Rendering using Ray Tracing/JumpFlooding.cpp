#include "JumpFlooding.h"

enum ShaderLocation {
	SL_CoordTex = 0,
	SL_ColorTex = 1,
	SL_ScreenSize = 2,
	SL_Step = 3
};

enum ArrayName {
	// m_buf
	AN_BufferA = 0,
	AN_BufferB = 1,

	// m_tex
	AN_CoordA = 0,
	AN_ColorA = 1,
	AN_CoordB = 2,
	AN_ColorB = 3
};

JumpFlooding::JumpFlooding()
{
	// Shaders
	m_CPShader = new Shader("./shader/cpVS.glsl", "./shader/cpFS.glsl");
	m_JFShader = new Shader("./shader/jfVS.glsl", "./shader/jfFS.glsl");

	// FBO
	genFBO();
	genPBO();

	// Step
	while (m_maxStep * 2 < g_screenSize.cx ||
		m_maxStep * 2 < g_screenSize.cy) m_maxStep *= 2;
	//glFinish();
}

JumpFlooding::~JumpFlooding()
{
	// Shaders
	delete m_CPShader;
	delete m_JFShader;

	// FBO
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(4, m_tex);

	//glFinish();
}

void JumpFlooding::resetShader()
{
	delete m_CPShader;
	m_CPShader = new Shader("./shader/cpVS.glsl", "./shader/cpFS.glsl");

	delete m_JFShader;
	m_JFShader = new Shader("./shader/jfVS.glsl", "./shader/jfFS.glsl");
}

void JumpFlooding::render(const GLuint rt, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	//glClear(GL_COLOR_BUFFER_BIT);

	/* Render Color Position */ {
		glDrawBuffers(2, m_buf[AN_BufferA]);

		m_CPShader->use();
		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt);
		glUniform2f(0, (float)g_screenSize.cx, (float)g_screenSize.cy);

		glDrawArrays(GL_QUADS, 0, 4);

		glBindTexture(GL_TEXTURE_2D, 0);
		m_CPShader->unuse();
	}

	/* Render Jump Flooding */ {
		m_JFShader->use();

		// Texture 4개를 GL_TEXTURE0~3 에 등록
		for (int i = 0; i < 4; i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, m_tex[i]);
		}

		glUniform2f(SL_ScreenSize, (float)g_screenSize.cx, (float)g_screenSize.cy);

		int step = m_maxStep;

		bool usingA = true;

		while (step >= 1) {
			glUniform1f(SL_Step, (float)step);

			if (usingA) {
				glDrawBuffers(2, m_buf[AN_BufferB]); // 출력은 AT2, AT3 로
				glUniform1i(SL_CoordTex, 0); // coordTex는 GL_TEXTURE0에 등록된 텍스처랑 연결
				glUniform1i(SL_ColorTex, 1); // colorTex는 GL_TEXTURE1에 등록된 텍스처랑 연결
			}
			else {
				glDrawBuffers(2, m_buf[AN_BufferA]); // 출력은 AT0, AT1 로
				glUniform1i(SL_CoordTex, 2); // coordTex는 GL_TEXTURE2에 등록된 텍스처랑 연결
				glUniform1i(SL_ColorTex, 3); // colorTex는 GL_TEXTURE3에 등록된 텍스처랑 연결
			}

			glDrawArrays(GL_QUADS, 0, 4);

			step /= 2;

			usingA = !usingA;
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_JFShader->unuse();

		// 결과를 저장한다.
		if (usingA) {
			m_coordTex = m_tex[AN_CoordA];
			m_colorTex = m_tex[AN_ColorA];
		}
		else {
			m_coordTex = m_tex[AN_CoordB];
			m_colorTex = m_tex[AN_ColorB];
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	//glFinish();

	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);
}

void JumpFlooding::genFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	// Check 4 textures.
	glGenTextures(4, m_tex);
	for (int i = 0; i < 4; i++) {
		// store size
		glBindTexture(GL_TEXTURE_2D, m_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
		glBindTexture(GL_TEXTURE_2D, 0);
		// attach
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_tex[i], 0);
	}

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//glFinish();
}

void JumpFlooding::genPBO() {
	glGenBuffers(2, m_pbo);
	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[i]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, g_screenSize.cx*g_screenSize.cy * 4, 0, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
}