#include "SibsonInterpolation.h"

SibsonInterpolation::SibsonInterpolation()
{
	// Shaders
	m_SIShader = new Shader("./shader/sibsonVS.glsl", "./shader/sibsonFS.glsl");

	// FBO
	genFBO();
}

SibsonInterpolation::~SibsonInterpolation()
{
	// Shaders
	delete m_SIShader;

	// FBO
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(1, &m_outputTex);
}

void SibsonInterpolation::resetShader()
{
	delete m_SIShader;
	m_SIShader = new Shader("./shader/sibsonVS.glsl", "./shader/sibsonFS.glsl");
}

void SibsonInterpolation::render(const GLuint coord, const GLuint color, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glDrawBuffers(1, m_buffers);

	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, coord); // accum(:1)
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, color); // accum(:1)

	m_SIShader->use();
	glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);

	glDrawArrays(GL_QUADS, 0, 4);
	m_SIShader->unuse();

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);
}

void SibsonInterpolation::genFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	// Check 3 textures.
	glGenTextures(1, &m_outputTex);
	glBindTexture(GL_TEXTURE_2D, m_outputTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTex, 0);

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//glFinish();
}