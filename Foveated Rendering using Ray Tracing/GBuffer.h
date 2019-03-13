#pragma once

//gl
#include <gl/glew.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <GL/GL.h>
#include <GL/GLU.h>
#include <vector>
#include "Shader.h"

struct Sphere;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
	Vertex() {};
	Vertex(glm::vec3 p, glm::vec3 n, glm::vec2 t)
	{
		position = p;
		normal = n;
		texCoord = t;
	}
};
class GBuffer
{
	Shader *m_GBShader = nullptr;
	GLuint m_cameraUBO = 0;
	GLuint m_fbo = 0;

	GLuint m_positionTex = 0;
	GLuint m_normalTex = 0;
	GLuint m_depthTex = 0;

	// Position, Normal, Depth
	GLenum m_buffers[3] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2};

public:
	GBuffer();
	~GBuffer();
	void render(const GLuint* qeury_pram, GLuint64* elapsed_time, int* done);
	void BindBuffers();
	void resetFrameCount();
	void resetShader();

	void genScene();
	void loadMesh(const char* fileName);
	void setupMesh();
private:
	void genCameraUBO();
	void genFBO();

	GLuint VAO, VBO, EBO;
	std::vector<Vertex> vertex;
	std::vector<GLuint> indices;

public:
	const GLuint& positionTex = m_positionTex;
	const GLuint& normalTex = m_normalTex;
	const GLuint& depthTex = m_depthTex;
	const GLuint& fbo = m_fbo;
};