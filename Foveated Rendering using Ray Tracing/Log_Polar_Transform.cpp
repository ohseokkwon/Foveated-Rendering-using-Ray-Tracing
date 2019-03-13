#include "Log_Polar_Transform.h"

#include "ATrous.h"

LogPolarTransform::LogPolarTransform()
{
	m_LPScale = glm::vec2(0.25f);
	m_LPbufferSize = glm::ivec2(g_screenSize.cx * m_LPScale.x, g_screenSize.cy * m_LPScale.y);

	// Shader
	m_LPInverseShader = new Shader(nullptr, "./shader/ilogPolarCPFS.glsl", true);
	m_LPForwardShader = new Shader(nullptr, "./shader/logPolarCPFS.glsl", true);

	// FBO
	genFBO();
	//glFinish();
}

LogPolarTransform::~LogPolarTransform()
{
	// Shder
	delete m_LPForwardShader;

	// FBO
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(2, m_tex);

	//glFinish();
}

void LogPolarTransform::resetShader()
{
	delete m_LPInverseShader;
	m_LPInverseShader = new Shader(nullptr, "./shader/ilogPolarCPFS.glsl", true);

	delete m_LPForwardShader;
	m_LPForwardShader = new Shader(nullptr, "./shader/logPolarCPFS.glsl", true);
}

void LogPolarTransform::render(GLuint pt, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffers(2, m_tex);

		// 로그폴라 변환
		{
			//glDrawBuffers(1, &m_tex[0]);
			glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pt);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			m_LPForwardShader->use();

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tex[0]);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_tex[0]);

			glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
			glUniform2f(2, (float)m_LPbufferSize.x, (float)m_LPbufferSize.y);
			glUniform2f(3, (float)m_LPScale.x, (float)m_LPScale.y);
			glUniform2f(4, (float)g_gaze.x, (float)(g_screenSize.cy - g_gaze.y));

			glUniform1i(glGetUniformLocation(m_LPForwardShader->getProgramID(), "destTex"), 3);
			glDispatchCompute(g_screenSize.cx / 32, g_screenSize.cy / 32, 1);

			glBindTexture(GL_TEXTURE_2D, 0);
			m_LPForwardShader->unuse();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
		

		// 역 로그폴라 변환
		{
			//glDrawBuffers(1, &m_tex[1]);
			glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pt);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			m_LPInverseShader->use();

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tex[1]);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_tex[1]);

			glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
			glUniform2f(2, (float)m_LPbufferSize.x, (float)m_LPbufferSize.y);
			glUniform2f(3, (float)m_LPScale.x, (float)m_LPScale.y);
			glUniform2f(4, (float)g_gaze.x, (float)(g_screenSize.cy - g_gaze.y));

			glUniform1i(glGetUniformLocation(m_LPInverseShader->getProgramID(), "destTex"), 4);
			glDispatchCompute(g_screenSize.cx / 32, g_screenSize.cy / 32, 1);

			glBindTexture(GL_TEXTURE_2D, 0);
			m_LPInverseShader->unuse();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
		
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);
}

void LogPolarTransform::genFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glGenTextures(2, m_tex);
	for (int i = 0; i < 2; i++) {
		// store size
		glBindTexture(GL_TEXTURE_2D, m_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
		glBindTexture(GL_TEXTURE_2D, 0);
		// attach
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_tex[i], 0);
		glBindImageTexture(i+3, m_tex[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
	}

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//glFinish();
}
