#include "Shader.h"
#include <cstdio>

Shader::~Shader()
{
	if (m_id != 0)
		glDeleteProgram(m_id);
}

Shader::Shader(const char * vertFile, const char * fragFile, const bool isCompute)
{
	if(!isCompute)
		m_id = genProgram(vertFile, nullptr, nullptr, nullptr, fragFile);
	else
		m_id = genProgram(fragFile);
}

Shader::Shader(const char * vertFile, const char * geomFile, const char * fragFile)
{
	m_id = genProgram(vertFile, nullptr, nullptr, geomFile, fragFile);
}

Shader::Shader(const char * vertFile, const char * tessContFile, const char * tessEvalFile, const char * fragFile)
{
	m_id = genProgram(vertFile, tessContFile, tessEvalFile, nullptr, fragFile);
}

Shader::Shader(const char * vertFile, const char * tessContFile, const char * tessEvalFile, const char * geomFile, const char * fragFile)
{
	m_id = genProgram(vertFile, tessContFile, tessEvalFile, geomFile, fragFile);
}

GLuint Shader::getProgramID() const
{
	return m_id;
}

void Shader::use() const
{
	glUseProgram(m_id);
}

void Shader::unuse() const
{
	glUseProgram(0);
}

GLuint Shader::genShader(GLenum type, const char * file)
{
	if (file == nullptr)
		return (GLuint)0;

	//파일을 읽고 소스를 str에 담는다.
	FILE* fin;
	fopen_s(&fin, file, "rt");
	if (!fin) {
		printf("There is not %s\n", file);
		return 0;
	}
	fseek(fin, 0, SEEK_END);
	const int length = ftell(fin);

	char *str = new char[length] {};

	//소스를 기반으로 세이더를 만듭니다.
	fseek(fin, 0, SEEK_SET);
	fread_s(str, length, length, 1, fin);
	fclose(fin);

	GLuint id = glCreateShader(type);
	const char* source = str;
	glShaderSource(id, 1, &source, &length);
	glCompileShader(id);

	delete[] str;

	//컴파일 실패면 세이더를 삭제합니다.
	int compile_checker;
	glGetShaderiv(id, GL_COMPILE_STATUS, &compile_checker);
	if (compile_checker == GL_FALSE) {
#ifdef _DEBUG
		GLchar infoLog[512];
		glGetShaderInfoLog(id, 512, NULL, infoLog);
		printf_s("Compile Fail: ");
		puts(file);
		puts(infoLog);
#endif
		glDeleteShader(id);
		return 0;
	}

	return id;
}

GLuint Shader::genProgram(const char* computeFragFile)
{
	GLuint computeFragID = genShader(GL_COMPUTE_SHADER, computeFragFile);

	GLuint id = glCreateProgram();
	if (computeFragID != 0) glAttachShader(id, computeFragID);
	glLinkProgram(id);

	if (computeFragID != 0) { glDetachShader(id, computeFragID);	glDeleteShader(computeFragID); }

	//링크실패면 프로그램을 삭제합니다.
	GLint link_checker;
	glGetProgramiv(id, GL_LINK_STATUS, &link_checker);
	if (link_checker == GL_FALSE) {
#ifdef _DEBUG
		GLchar infoLog[512];
		glGetProgramInfoLog(id, 512, NULL, infoLog);
		printf_s("Link Fail: ");
		puts(infoLog);
#endif
		glDeleteProgram(id);
		return 0;
	}

	return id;
}

GLuint Shader::genProgram(const char * vertFile, const char * tessContFile, const char * tessEvalFile, const char * geomFile, const char * fragFile)
{
	//세이더들을 만든다.
	GLuint vertID = genShader(GL_VERTEX_SHADER, vertFile);
	GLuint tescID = genShader(GL_TESS_CONTROL_SHADER, tessContFile);
	GLuint teseID = genShader(GL_TESS_EVALUATION_SHADER, tessEvalFile);
	GLuint geomID = genShader(GL_GEOMETRY_SHADER, geomFile);
	GLuint fragID = genShader(GL_FRAGMENT_SHADER, fragFile);

	//프로그램을 만들고 링크합니다.
	GLuint id = glCreateProgram();
	if (vertID != 0) glAttachShader(id, vertID);
	if (tescID != 0) glAttachShader(id, tescID);
	if (teseID != 0) glAttachShader(id, teseID);
	if (geomID != 0) glAttachShader(id, geomID);
	if (fragID != 0) glAttachShader(id, fragID);
	glLinkProgram(id);

	//세이들을 삭제합니다.
	if (vertID != 0) { glDetachShader(id, vertID);	glDeleteShader(vertID); }
	if (tescID != 0) { glDetachShader(id, tescID);	glDeleteShader(tescID); }
	if (teseID != 0) { glDetachShader(id, teseID);	glDeleteShader(teseID); }
	if (geomID != 0) { glDetachShader(id, geomID);	glDeleteShader(geomID); }
	if (fragID != 0) { glDetachShader(id, fragID);	glDeleteShader(fragID); }

	//링크실패면 프로그램을 삭제합니다.
	GLint link_checker;
	glGetProgramiv(id, GL_LINK_STATUS, &link_checker);
	if (link_checker == GL_FALSE) {
#ifdef _DEBUG
		GLchar infoLog[512];
		glGetProgramInfoLog(id, 512, NULL, infoLog);
		printf_s("Link Fail: ");
		puts(infoLog);
#endif
		glDeleteProgram(id);
		return 0;
	}

	return id;
}
