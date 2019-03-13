#include "ATrous.h"

enum ShaderLocation {
	SL_ColorTex = 0,
	SL_C_Phi = 1,
	SL_N_Phi = 2,
	SL_P_Phi = 3,
	SL_StepWidth = 4,
	SL_ScreenSize = 5,
	SL_EyeCoord = 6,
	SL_ApectureSize = 7,
};

enum ArrayName {
	// m_buf
	AN_BufferA = 0,
	AN_BufferB = 1,

	// m_tex
	AN_ColorA = 0,
	AN_ColorB = 1
};

ATrous::ATrous()
{
	// Shader
	m_ATShader = new Shader("./shader/atVS.glsl", "./shader/atFS.glsl");

	// FBO
	genFBO();

	//glFinish();
}

ATrous::~ATrous()
{
	// Shder
	delete m_ATShader;

	// FBO
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(2, m_tex);

	//glFinish();
}

void ATrous::render(int count, GLuint pt, GLuint nt, GLuint ct, GLuint rt, const int frame, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);
	// fbo
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	float c_phi = 1.f;
	float n_phi = 1.f;
	float p_phi = 1.f;
	int stepWidth = 1;

	m_ATShader->use();
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pt);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, nt);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, rt);

	glUniform2f(SL_ScreenSize, (float)g_screenSize.cx, (float)g_screenSize.cy);
	glUniform2f(SL_EyeCoord, (float)g_gaze.x, (float)(g_screenSize.cy - g_gaze.y));
	//glUniform2f(SL_EyeCoord, (float)g_screenSize.cx*0.5f, (float)(g_screenSize.cy - g_screenSize.cy*0.5f));
	glUniform1f(SL_ApectureSize, g_apertureSize * 1.0f);

	/* ct를 입력을 사용해서 Buffer A에 씁니다. */ {
		glDrawBuffers(1, m_buf[AN_BufferA]); // A에 출력
		glUniform1i(SL_ColorTex, 2); // colorTex를 GL_TEXTURE2로 연결, ct도 TEX2로 연결.
		glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, ct); // ct 사용

		glUniform1f(SL_C_Phi, c_phi);
		glUniform1f(SL_N_Phi, n_phi);
		glUniform1f(SL_P_Phi, p_phi);
		glUniform1f(SL_StepWidth, (float)stepWidth);

		glDrawArrays(GL_QUADS, 0, 4);

		count -= 1;
	}

	/* Buffer A를 먼저 입력으로 사용합니다. */
	bool usingA = true;

	/* Render 'count - 1' times */ {
		glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_tex[AN_ColorA]);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_tex[AN_ColorB]);

		while (count--) {
			c_phi *= 1.0f;
			n_phi *= 0.5f;
			p_phi *= 1.0f;
			stepWidth *= 2;

			glUniform1f(SL_C_Phi, c_phi);
			glUniform1f(SL_N_Phi, n_phi);
			glUniform1f(SL_P_Phi, p_phi);
			glUniform1f(SL_StepWidth, (float)stepWidth);

			if (usingA) {
				glDrawBuffers(1, m_buf[AN_BufferB]); // B에 출력
				glUniform1i(SL_ColorTex, 2); // A를 입력
			}
			else {
				glDrawBuffers(1, m_buf[AN_BufferA]); // A에 출력
				glUniform1i(SL_ColorTex, 3); // B를 입력
			}

			glDrawArrays(GL_QUADS, 0, 4);

			usingA = !usingA;
		}
	}

	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
	m_ATShader->unuse();

	// 결과를 저장한다.
	if (usingA)
		m_colorTex = m_tex[AN_ColorA];
	else
		m_colorTex = m_tex[AN_ColorB];

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	//glFinish();
	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);
}

void ATrous::genFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glGenTextures(2, m_tex);
	for (int i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D, m_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
		glBindTexture(GL_TEXTURE_2D, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_tex[i], 0);
	}

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//glFinish();
}
