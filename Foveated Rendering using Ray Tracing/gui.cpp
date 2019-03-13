#include "gui.h"
#include <cstdio>

/********************************************/
// Global Variable
/********************************************/
Camera g_camera;
bool g_camera_moved = false;
POINT g_gaze = POINT{ 0, 0 };
SIZE g_screenSize;

float g_aspect;
float g_apertureSize = 0.07f;
GLuint* g_tex = 0;
int g_isTextureChanged = 0;
int g_isOptimize = 1;
int g_saveBMP = 0;
float g_number_of_ray = 0.0f;
float g_FPS = 0.0f;
bool g_shader_init = false;
float g_light_Power = 810.0f;
bool g_light_changed = false;

bool g_fullScreen = false;

int g_diffuse_max_depth = 1;

/********************************************/
// Functions
/********************************************/

void framebufferSizeCallback(GLFWwindow*, int w, int h)
{
	g_gaze.x = w / 2;
	g_gaze.y = h / 2;
	g_screenSize.cx = w;
	g_screenSize.cy = h == 0 ? 1 : h;

	g_aspect = (float)g_screenSize.cx / (float)g_screenSize.cy;

	g_screenSize.cx = (uint32_t)w;
	g_screenSize.cy = h == 0 ? 1 : (uint32_t)h;

	g_camera.setScreen(glm::vec2(g_screenSize.cy, g_screenSize.cy));
	g_camera.setViewport(glm::vec4(0, 0, g_screenSize.cx, g_screenSize.cy));
}

void cursorPosCallback(GLFWwindow * window, double xpos, double ypos)
{
	static glm::vec2 prevPos(-1, -1);
	float adjust_scale = 1.25f;
	if (g_fullScreen)
		adjust_scale = 1.f;
	else
		adjust_scale = 1.25f;

	if (prevPos.x < 0) {
		prevPos = glm::vec2((float)xpos, (float)ypos * adjust_scale);
		g_gaze.x = xpos;
		g_gaze.y = ypos  * adjust_scale;
		return;
	}

	// 공유 마우스 위치
	g_gaze.x = xpos;
	g_gaze.y = ypos * adjust_scale;

	// 고정 마우스 위치
	//g_gaze.x = g_screenSize.cx / 2;
	//g_gaze.y = g_screenSize.cy / 2;

	glm::vec2 pos((float)xpos, (float)ypos);
	float dx = (pos.x - prevPos.x) / g_screenSize.cx;
	float dy = (prevPos.y - pos.y) / g_screenSize.cy;

	//if (!g_camera.isCursorInViewport(pos))
	//	return;

	g_camera_moved = false;

	// 왼쪽 클릭 상태 (카메라 회전)
	if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
		g_camera.rotate(dx * glm::pi<float>(), glm::vec3(0, -1, 0));
		g_camera.rotate(dy * glm::pi<float>(), g_camera.getRight());

		//g_camera.lookAt(g_camera.getPosition() + g_camera.getFront());

		g_camera_moved = true;
	}

	// 가운데 클릭 상태 (줌인)
	if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
		g_camera.translate(g_camera.getFront() * (dx + dy) * 100.0f);

		g_camera_moved = true;
	}

	// 오른쪽 클릭 상태 (중심을 기준으로 회전)
	if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
		glm::vec3 target = g_camera.getTarget();
		g_camera.rotateAround(target, dx * glm::pi<float>(), glm::vec3(0, -1, 0));
		g_camera.rotateAround(target, dy * glm::pi<float>(), g_camera.getRight());
		//g_camera.lookAt(g_camera.getPosition() + g_camera.getFront());

		g_camera_moved = true;
	}

	prevPos = pos;

	glm::vec3 origin = g_camera.getPosition();
	printf("cam_orig = %f, %f, %f \n", origin.x, origin.y, origin.z);
	glm::vec3 target = g_camera.getTarget();
	printf("cam_target = %f, %f, %f \n", target.x, target.y, target.z);
	glm::vec3 up = g_camera.getUp();
	printf("cam_up = %f, %f, %f \n", up.x, up.y, up.z);
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	const int btnCode = GLFW_KEY_1;
	if (action == GLFW_PRESS) {
		switch (key) {
		case btnCode + 0: g_isTextureChanged = 1;  break;
		case btnCode + 1: g_isTextureChanged = 2;  break;
		case btnCode + 2: g_isTextureChanged = 3;  break;
		case btnCode + 3: g_isTextureChanged = 4;  break;
		case btnCode + 4: g_isTextureChanged = 5;  break;
		case btnCode + 5: g_isTextureChanged = 6;  break;
		case btnCode + 6: g_isTextureChanged = 7;  break;
		case btnCode + 7: g_isTextureChanged = 8;  break;
		case btnCode + 8: g_isTextureChanged = 9;  break;
		
		case GLFW_KEY_HOME: g_saveBMP = true; break;
		case GLFW_KEY_PAGE_UP:
			g_apertureSize += 0.01f;
			if (g_apertureSize > 5.0f)
				g_apertureSize = 5.0f;
			break;
		case GLFW_KEY_PAGE_DOWN:
			g_apertureSize -= 0.01f;
			if (g_apertureSize < 0.0f)
				g_apertureSize = 0.0f;
			break;

		case GLFW_KEY_ENTER:
			g_shader_init = true;
			break;
		case GLFW_KEY_F1:
			g_fullScreen = 1-g_fullScreen;
			break;
		case GLFW_KEY_SPACE:
			g_isOptimize = 1 - g_isOptimize;
			break;
		}
	}

	switch (key) {
	case GLFW_KEY_UP:
		g_camera.translate(g_camera.getFront() * g_FPS * 0.1f);
		g_camera_moved = true;

		break;
	case GLFW_KEY_DOWN:
		g_camera.translate(g_camera.getBack() * g_FPS * 0.1f);
		g_camera_moved = true;
		break;
	case GLFW_KEY_RIGHT:
		g_light_changed = true;
		g_light_Power += 10.0f;
		break;
	case GLFW_KEY_LEFT:
		g_light_changed = true;
		g_light_Power -= 10.0f;
		if (g_light_Power < 0.0f)
			g_light_Power = 0.0f;
		break;

	case GLFW_KEY_KP_ADD:
		g_diffuse_max_depth++;
		break;
	case GLFW_KEY_KP_SUBTRACT:
		g_diffuse_max_depth--;
		if (g_diffuse_max_depth < 1)
			g_diffuse_max_depth = 1;
		break;

	case GLFW_KEY_ESCAPE:
		exit(0);
		break;
	}
}

void initContext(bool useDefault, int major, int minor, bool useCompatibility)
{
	if (useDefault)
	{
		// 기본적으로  Default로 설정해주면
		// 가능한 최신의 OpenGL Vesion을 선택하며 (연구실 컴퓨터는 4.5)
		// Profile은 Legacy Function도 사용할 수 있게 해줍니다.(Compatibility 사용)
		glfwDefaultWindowHints();
	}
	else
	{
		// Major 와 Minor 결정
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);

		// Core 또는 Compatibility 선택
		if (useCompatibility)
		{
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
		}
		else
		{
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		}
	}
}

int printAllErrors(const char * caption /*= nullptr*/)
{
	if (caption)
		puts(caption);

	int err;
	int err_count = 0;

	do {
		// 에러 출력
		err = glGetError();
		printf(" Error: %s\n", getGLErrorStr(err));
		err_count++;
	} while (err != GL_NO_ERROR);

	return err_count - 1;
}

const char * getGLErrorStr(GLenum err)
{
	switch (err)
	{
	case GL_NO_ERROR:          return "No error";
	case GL_INVALID_ENUM:      return "Invalid enum";
	case GL_INVALID_VALUE:     return "Invalid value";
	case GL_INVALID_OPERATION: return "Invalid operation";
	case GL_STACK_OVERFLOW:    return "Stack overflow";
	case GL_STACK_UNDERFLOW:   return "Stack underflow";
	case GL_OUT_OF_MEMORY:     return "Out of memory";
	default:                   return "Unknown error";
	}
}

GLuint loadBmp(const char * file, const int channel)
{
	FILE* fin = nullptr;
	fopen_s(&fin, file, "rb");
	if (fin == nullptr)
		return 0;

#pragma pack(push, 2) //struct를 2바이트씩 처리하겠다는 뜻.
	struct {
		uint16_t	type;
		uint64_t				_unused1;
		uint32_t	offset;
		uint32_t				_unused2;
		int32_t		width;
		int32_t		height;
		uint16_t				_unused3;
		uint16_t	bitCount;
	} bh; //Bitmap Header
#pragma pack(pop)

	if (1 != fread_s(&bh, sizeof(bh), sizeof(bh), 1, fin)) {
		fclose(fin);
		return 0;
	}
	if (bh.type != 0x4D42 || bh.bitCount != channel * 8 || bh.height < 0) {
		fclose(fin);
		return 0;
	}

	int dl = bh.width * bh.bitCount / 8; //data length
	int pl = (dl % 4 != 0) ? (4 - dl % 4) : 0; //padding length

	std::vector<uint8_t> data(bh.height * dl);

	//데이터 채우기
	fseek(fin, bh.offset, SEEK_SET);
	for (int h = 0; h < bh.height; h++) {
		fread_s(data.data() + h*dl, dl, dl, 1, fin);
		fseek(fin, pl, SEEK_CUR);
	}
	fclose(fin);

	float bordercolor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	//생성
	GLuint id;
	glGenTextures(1, &id);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bordercolor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0, channel == 3 ? GL_RGB32F : GL_RGBA32F, bh.width, bh.height, 0, channel == 3 ? GL_BGR : GL_BGRA, GL_UNSIGNED_BYTE, data.data());
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	return id;
}

void saveBMP24(const char * fileName, int* idx)
{
	char *readBuffer = new char[g_screenSize.cx * g_screenSize.cy * 4];
	glReadPixels(0, 0, g_screenSize.cx, g_screenSize.cy, GL_BGRA, GL_UNSIGNED_BYTE, readBuffer);

	BITMAPINFOHEADER bi;
	BITMAPFILEHEADER bf;

	bf.bfType = 19778;
	bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bf.bfSize = g_screenSize.cx * g_screenSize.cy * 4 + bf.bfOffBits;
	bf.bfReserved1 = bf.bfReserved2 = 0;

	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = g_screenSize.cx;
	bi.biHeight = g_screenSize.cy;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = g_screenSize.cx * g_screenSize.cy * 4;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	bi.biXPelsPerMeter = bi.biYPelsPerMeter = 0;

	FILE *fp;
	char saveFileName[128] = { 0 };
	CreateDirectoryA("../Compare", nullptr);
	if (idx != nullptr) {
		sprintf_s(saveFileName, "../Compare/comp3/%s%d.bmp", fileName, *idx); (*idx)+=1;
	}
	else {
		sprintf_s(saveFileName, "../Compare/comp3/%s.bmp", fileName);
	}
	fopen_s(&fp, saveFileName, "wb");
	fwrite(&bf, sizeof(BITMAPFILEHEADER), 1, fp);
	fwrite(&bi, sizeof(BITMAPINFOHEADER), 1, fp);
	fwrite(readBuffer, 1, bi.biWidth * bi.biHeight * 4, fp);
	fclose(fp);

	delete[] readBuffer;
}