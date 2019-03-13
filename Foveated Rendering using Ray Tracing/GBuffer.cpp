#include "GBuffer.h"
#include "gui.h"
#include <random>
#include <chrono>
#include <iostream>
#include <istream>
#include <fstream>



GBuffer::GBuffer()
{
	/* Shader */
	m_GBShader = new Shader("./shader/gbufferVS.glsl", "./shader/gbufferFS.glsl");

	///* load OBJ UBO */
	//genScene();

	/////* Camera UBO */
	//genCameraUBO();

	/////* FBO */
	//genFBO();

	//glFinish();
}

GBuffer::~GBuffer()
{
	/* Shader */
	delete m_GBShader;

	/* FBO */
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(1, &m_positionTex);
	glDeleteTextures(1, &m_normalTex);
	glDeleteTextures(1, &m_depthTex);
}

void swap(GLuint *a, GLuint *b) {
	GLuint tmp = *b;
	*b = *a;
	*a = tmp;
}
void GBuffer::render(const GLuint* qeury_pram, GLuint64* elapsed_time, int* done)
{
	glBeginQuery(GL_TIME_ELAPSED, *qeury_pram);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glDrawBuffers(3, m_buffers); //position(:0), normal(:1), accum(:2), color(:3)

	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_positionTex); // accum(:1)
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_normalTex); // accum(:1)
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_depthTex); // accum(:1)

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glFrontFace(GL_CW);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
// 	glMatrixMode(GL_PROJECTION);
// 	glLoadIdentity();
// 	glm::mat4 pmat = g_camera.getPMat();
// 	glMultMatrixf(&pmat[0][0]);
// 
// 	glMatrixMode(GL_MODELVIEW);
// 	glLoadIdentity();
// 	glm::mat4 vmat = g_camera.getVMat();
// 	glMultMatrixf(&vmat[0][0]);

	// camera ubo 재구성
	BindBuffers();

	// shader 시작
	m_GBShader->use();
	

	glUniform2f(3, (float)g_screenSize.cx, (float)g_screenSize.cy);
	// rendering
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	//glDrawArrays(GL_QUADS, 0, 4);

	m_GBShader->unuse();
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	//glFinish();
	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectiv(*qeury_pram, GL_QUERY_RESULT_AVAILABLE, done);
	glGetQueryObjectui64v(*qeury_pram, GL_QUERY_RESULT, elapsed_time);
}

void GBuffer::BindBuffers()
{
	/* 카메라 데이터 변경 */
	glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
	glm::vec3 pos = g_camera.getPosition();
	glBufferSubData(GL_UNIFORM_BUFFER, 0, 12, &pos[0]);
	glBufferSubData(GL_UNIFORM_BUFFER, 16, 64, g_camera.pmat());
	glBufferSubData(GL_UNIFORM_BUFFER, 80, 64, g_camera.vmat());
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	//glBindVertexArray(VAO);
	//glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(Vertex),
	//	&vertex[0], GL_STATIC_DRAW);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
	//	&indices[0], GL_STATIC_DRAW);
	//// Vertex Positions
	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	//	(GLvoid*)0);
	//// Vertex Normals
	//glEnableVertexAttribArray(1);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	//	(GLvoid*)offsetof(Vertex, normal));
	//// Vertex Texture Coords
	//glEnableVertexAttribArray(2);
	//glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	//	(GLvoid*)offsetof(Vertex, texCoord));
	//glBindVertexArray(0);

	//glFinish();
}

void GBuffer::resetFrameCount()
{
}

void GBuffer::resetShader()
{
	delete m_GBShader;
	m_GBShader = new Shader("./shader/gbufferVS.glsl", "./shader/gbufferFS.glsl");
}

void GBuffer::genScene()
{
	//loadMesh((g_obj_name + ".obj").c_str());
}

void GBuffer::loadMesh(const char* fileName)
{
	using namespace std;

	int i, j, number1, number2, number3, indNormal1, indNormal2, indNormal3, indTC1, indTC2, indTC3, nrFace = 0, nrInd = -1;
	char text[256], matName[256], path[256];

	vector<string>tmp;
	vector<string>line;
	vector<glm::vec3>position;
	vector<glm::vec3>normal;
	vector<glm::vec2>texCoord;
	vector<glm::vec3>vertexNormal;
	vector<glm::vec2>texCoordCopy;
	ifstream fin;
	string imageName;
	glm::vec3 v;
	glm::vec2 t;

	fin.open(fileName);
	while (!fin.eof())
	{
		fin.getline(text, 256);
		line.push_back(text);
	}
	fin.close();

	string matFileName;
	matFileName = fileName;
	matFileName.resize(matFileName.size() - 3);
	matFileName += "mtl";

	fin.open(matFileName.c_str());
	while (!fin.eof())
	{
		fin.getline(text, 200);
		tmp.push_back(text);
	}
	fin.close();
	//vector<int>face;

	for (i = 3; i < line.size(); i++)///READ THE MESHES
	{
		if (line[i][0] == 'v'&&line[i][1] == ' ')
		{
			sscanf_s(line[i].c_str(), "v %f %f %f", &v.x, &v.y, &v.z);
			position.push_back(v);
		}
		else if (line[i][0] == 'v'&&line[i][1] == 't')
		{
			sscanf_s(line[i].c_str(), "vt %f %f", &t.x, &t.y);
			texCoord.push_back(t);
		}
		else if (line[i][0] == 'v'&&line[i][1] == 'n')
		{
			sscanf_s(line[i].c_str(), "vn %f %f %f", &v.x, &v.y, &v.z);
			normal.push_back(v);
		}
		else if (line[i][0] == 's'&&line[i][1] == ' ')
			continue;
		else if (line[i][0] == 'u'&&line[i][1] == 's'&&line[i][2] == 'e')
		{
			sscanf_s(line[i].c_str(), "usemtl %s", &matName, sizeof(matName));
			//face.push_back(nrFace);
		}
		else if (line[i][0] == 'f'&&line[i][1] == ' ')
		{
			nrFace++;
			sscanf_s(line[i].c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d", &number1, &indTC1, &indNormal1,
				&number2, &indTC2, &indNormal2,
				&number3, &indTC3, &indNormal3);
			indices.push_back(number1 - 1);
			indices.push_back(number3 - 1);
			indices.push_back(number2 - 1);

			if (position.size() > vertexNormal.size())
				vertexNormal.resize(position.size());
			if (position.size() > texCoordCopy.size())
				texCoordCopy.resize(position.size());
			vertexNormal[number1 - 1] = normal[indNormal1 - 1];
			vertexNormal[number2 - 1] = normal[indNormal2 - 1];
			vertexNormal[number3 - 1] = normal[indNormal3 - 1];
			texCoordCopy[number1 - 1] = texCoord[indTC1 - 1];
			texCoordCopy[number2 - 1] = texCoord[indTC2 - 1];
			texCoordCopy[number3 - 1] = texCoord[indTC3 - 1];
		}
	}
	//face.push_back(nrFace+1);

	for (i = 0; i < position.size(); i++)
		vertex.push_back(Vertex(position[i], vertexNormal[i], texCoordCopy[i]));
	setupMesh();
	fin.close();
}
void GBuffer::setupMesh()
{
	int i;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(Vertex),
		&vertex[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
		&indices[0], GL_STATIC_DRAW);
	// Vertex Positions
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		(GLvoid*)0);
	// Vertex Normals
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		(GLvoid*)offsetof(Vertex, normal));
	// Vertex Texture Coords
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		(GLvoid*)offsetof(Vertex, texCoord));
	glBindVertexArray(0);
}

void GBuffer::genCameraUBO()
{
	glGenBuffers(1, &m_cameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);

	glBufferData(GL_UNIFORM_BUFFER, 144, nullptr, GL_STATIC_DRAW);

	glm::vec3 pos = g_camera.getPosition();
	glBufferSubData(GL_UNIFORM_BUFFER, 0, 12, &pos[0]);
	glBufferSubData(GL_UNIFORM_BUFFER, 16, 64, g_camera.pmat());
	glBufferSubData(GL_UNIFORM_BUFFER, 80, 64, g_camera.vmat());
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// 1에 연결
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_cameraUBO);
}

void GBuffer::genFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	// position texture: color0
	glGenTextures(1, &m_positionTex);
	glBindTexture(GL_TEXTURE_2D, m_positionTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_positionTex, 0);

	// normal texture: color1
	glGenTextures(1, &m_normalTex);
	glBindTexture(GL_TEXTURE_2D, m_normalTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normalTex, 0);

	// depth texture: color4
	glGenTextures(1, &m_depthTex);
	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, g_screenSize.cx, g_screenSize.cy);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_depthTex, 0);

	// assert
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}