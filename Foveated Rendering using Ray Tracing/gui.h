#pragma once
#include <windows.h>
#include "Camera.h"
#include "Shader.h"

#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string.h>
#include <vector>

extern Camera g_camera;
extern bool g_camera_moved;
extern POINT g_gaze;
extern SIZE g_screenSize;
extern float g_aspect;
extern float g_apertureSize;
extern GLuint* g_tex;
extern int g_isTextureChanged;
extern int g_isOptimize;
extern GLuint g_time_query[10];
extern int g_saveBMP;
extern float g_number_of_ray;
extern float g_FPS;
extern float g_light_Power;
extern bool g_light_changed;
extern bool g_fullScreen;

extern int g_diffuse_max_depth;
extern bool g_shader_init;

// 함수 선언
int printAllErrors(const char * caption = nullptr);
const char * getGLErrorStr(GLenum err);
void initContext(bool useDefault, int major = 3, int minor = 3, bool useCompatibility = false);
void framebufferSizeCallback(GLFWwindow*, int w, int h);
void cursorPosCallback(GLFWwindow* window, double x, double y);
void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

GLuint loadBmp(const char * file, const int channel = 4);
void saveBMP24(const char * fileName, int* idx = nullptr);