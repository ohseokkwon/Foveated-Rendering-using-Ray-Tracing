#include "PullPushInterpolation.h"

enum OutTexture {
	// m_buf
	PULL_BufferA = GL_COLOR_ATTACHMENT0,
	PULL_BufferB = GL_COLOR_ATTACHMENT1,
};

PullPushInterpolation::PullPushInterpolation()
{
	m_bufferSize = glm::vec2(g_screenSize.cx * 1.5f, g_screenSize.cy);
	// Shaders
	m_PullShader = new Shader(nullptr, "./shader/pullFS.glsl", true);
	m_PushShader = new Shader(nullptr, "./shader/pushFS.glsl", true);
	m_FinalShader = new Shader(nullptr, "./shader/pullpushFinal.glsl", true);
	// FBO
	genFBO();
}

PullPushInterpolation::~PullPushInterpolation()
{
	// Shaders
	delete m_PullShader;
	delete m_PushShader;
	delete m_FinalShader;

	// FBO
	glDeleteFramebuffers(1, &m_fbo_pyramid);
	glDeleteTextures(1, &m_pull_Tex);
	glDeleteTextures(1, &m_push_Tex);

	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(1, &m_outputTex);
}

void PullPushInterpolation::resetShader()
{
	delete m_PullShader;
	m_PullShader = new Shader(nullptr, "./shader/pullFS.glsl", true);

	delete m_PushShader;
	m_PushShader = new Shader(nullptr, "./shader/pushFS.glsl", true);

	delete m_FinalShader;
	m_FinalShader = new Shader(nullptr, "./shader/pullpushFinal.glsl", true);
}

void PullPushInterpolation::render(const GLuint sparse, const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);

	// PULL Stage
	const int exponent = std::log2(g_screenSize.cx);
	int step = 0;
	const GLuint* ptr = nullptr;
	m_buffer = GL_COLOR_ATTACHMENT0;
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_pyramid);
	// PULL Stage
	{
		// 출력 버퍼는 PULL
		m_buffer = GL_COLOR_ATTACHMENT1;
		step = exponent;

		glm::vec2 searchOffset = glm::vec2(0, 0);
		glm::vec2 writeOffset = glm::vec2(0, 0);

		// PULL Once First step
		glDrawBuffers(1, &m_buffer);
		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sparse);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		m_PullShader->use();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pull_Tex);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pull_Tex);

		glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
		glUniform2fv(2, 1, &m_bufferSize[0]);
		glUniform2fv(3, 1, &searchOffset[0]);
		glUniform2fv(4, 1, &writeOffset[0]);
		glUniform1i(5, step);
		glUniform1i(6, -1);

		glUniform1i(glGetUniformLocation(m_PullShader->getProgramID(), "destTex"), 0);
		glDispatchCompute(m_bufferSize.x / 32, m_bufferSize.y / 32, 1); // 512^2 threads in blocks of 16^2

		m_PullShader->unuse();
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		step = exponent - 1;
		writeOffset = glm::vec2(g_screenSize.cx, pow(2, step) - 1);

		int count = 0;
		while (step > -1) {
			//if (count == 2)
			//	break;
			glDrawBuffers(1, &m_buffer);
			glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sparse);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			m_PullShader->use();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pull_Tex);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pull_Tex);

			glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
			glUniform2fv(2, 1, &m_bufferSize[0]);
			glUniform2fv(3, 1, &searchOffset[0]);
			glUniform2fv(4, 1, &writeOffset[0]);
			glUniform1i(5, step);
			glUniform1i(6, count);

			glUniform1i(glGetUniformLocation(m_PullShader->getProgramID(), "destTex"), 0);
			glDispatchCompute(m_bufferSize.x / 32, m_bufferSize.y / 32, 1); // 512^2 threads in blocks of 16^2

			m_PullShader->unuse();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			step--;
			count++;

			searchOffset.y -= pow(2, step - 1);
			writeOffset.y -= pow(2, step);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// PUSH Stage
	{
		// 출력 버퍼는 pushB
		m_buffer = GL_COLOR_ATTACHMENT2;
		const GLuint* pullTex = &m_pull_Tex;
		step = exponent;

		glm::vec2 searchOffset = glm::vec2(g_screenSize.cx, 0);
		glm::vec2 writeOffset = glm::vec2(g_screenSize.cx, 0);


		// push Once First step
		glDrawBuffers(1, &m_buffer);
		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sparse);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		m_PushShader->use();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_push_Tex);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_push_Tex);

		glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
		glUniform2fv(2, 1, &m_bufferSize[0]);
		glUniform2fv(3, 1, &searchOffset[0]);
		glUniform2fv(4, 1, &writeOffset[0]);
		glUniform1i(5, step);
		glUniform1i(6, -1);

		glUniform1i(glGetUniformLocation(m_PushShader->getProgramID(), "destTex"), 1);
		glDispatchCompute(m_bufferSize.x / 32, m_bufferSize.y / 32, 1); // 512^2 threads in blocks of 16^2

		m_PushShader->unuse();
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		writeOffset = glm::vec2(g_screenSize.cx, pow(2, 0));

		step = exponent - 1;
		int count = 1;
		while (step > -1) {
			//if (count == 2)
			//	break;
			glDrawBuffers(1, &m_buffer);
			glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sparse); // accum(:1)

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			m_PushShader->use();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_push_Tex);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_push_Tex);

			glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
			glUniform2fv(2, 1, &m_bufferSize[0]);
			glUniform2fv(3, 1, &searchOffset[0]);
			glUniform2fv(4, 1, &writeOffset[0]);
			glUniform1i(5, step);
			glUniform1i(6, count);

			glUniform1i(glGetUniformLocation(m_PushShader->getProgramID(), "destTex"), 1);
			glDispatchCompute(m_bufferSize.x / 32, m_bufferSize.y / 32, 1); // 512^2 threads in blocks of 16^2

			m_PushShader->unuse();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			step--;
			count++;

			searchOffset.y += pow(2, count - 1);
			writeOffset.y += pow(2, count - 1);
			if (step == 0)
				writeOffset = glm::vec2(0.0f);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		// end push
	}
	// push Final (pushA -> full resolution)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		m_buffer = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &m_buffer);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		m_FinalShader->use();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_outputTex);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_outputTex);

		glUniform2f(1, (float)g_screenSize.cx, (float)g_screenSize.cy);
		glUniform1i(glGetUniformLocation(m_PushShader->getProgramID(), "destTex"), 2);
		glDispatchCompute(m_bufferSize.x / 32, m_bufferSize.y / 32, 1); // 512^2 threads in blocks of 16^2

		m_FinalShader->unuse();
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}


	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);

	// 원위치 (t+1)을 위해서
	m_buffer = GL_COLOR_ATTACHMENT0;
	//GLenum tmp1 = m_buffers[0];
	//m_buffers[0] = m_buffers[1];
	//m_buffers[1] = tmp1;
}

void PullPushInterpolation::genFBO()
{
	float bordercolor[4] = { 0.0f };

	glGenFramebuffers(1, &m_fbo_pyramid);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo_pyramid);
	// pull textures.
	glGenTextures(1, &m_pull_Tex);
	glBindTexture(GL_TEXTURE_2D, m_pull_Tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx * 1.5f, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_pull_Tex, 0);
	glBindImageTexture(0, m_pull_Tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

	// push textures.
	glGenTextures(1, &m_push_Tex);
	glBindTexture(GL_TEXTURE_2D, m_push_Tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx * 1.5f, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_push_Tex, 0);
	glBindImageTexture(1, m_push_Tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
	glBindImageTexture(2, m_outputTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

	// assert
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//glFinish();
}