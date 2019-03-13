#include "gui.h"
#include "PathTracer.h"

#include <sutil.h>
#include <time.h>
#include <iostream>
#include <fstream>

#include "ATrous.h"
#include "JumpFlooding.h"
#include "PullPushInterpolation.h"
#include "SibsonInterpolation.h"
#include "GBuffer.h"
#include "Log_Polar_Transform.h"

GLuint g_time_query[10];
std::ofstream g_fout;
float g_viewScale = 1.0f;
FILE *g_fp;

void PrintMSTimes(std::string str, std::string& dst, GLuint64 ms, clock_t &total_ms) {
	total_ms += (ms / 1e6);
	dst += str + ", " + std::to_string((ms / 1e6)) + ", ";
}

void renderAll(const PathTracer *tracer, const GLuint *tex) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, g_screenSize.cx, g_screenSize.cy);
	glEnable(GL_TEXTURE_2D);

	static GLuint quad = 0;

	if (!quad) {
		quad = glGenLists(1);
		glNewList(quad, GL_COMPILE);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);	glVertex2f(0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);	glVertex2f(1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);   glVertex2f(1.0f, 1.0f);
		glTexCoord2f(0.0f, 1.0f);   glVertex2f(0.0f, 1.0f);
		glEnd();
		glEndList();
	}

	//4등분
#if 0
	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::POSITION));
	glPushMatrix();
	glTranslatef(0.0f, 0.5f, 0.f);
	glScalef(0.5f, 0.5f, 1.f);
	glCallList(quad);
	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::NORMAL));
	glPushMatrix();
	glTranslatef(0.5f, 0.5f, 0.f);
	glScalef(0.5f, 0.5f, 1.f);
	glCallList(quad);
	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::DEPTH));
	glPushMatrix();
	glTranslatef(0.0f, 0.0f, 0.f);
	glScalef(0.5f, 0.5f, 1.f);
	glCallList(quad);
	glPopMatrix();

	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::DIFFUSE));
	glPushMatrix();
	glTranslatef(0.5f, 0.0f, 0.f);
	glScalef(0.5f, 0.5f, 1.f);
	glCallList(quad);
	glPopMatrix();
#else
	// 색
	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::WEIGHT));
	glPushMatrix();
	glTranslatef(0.0f, 0.0f, 0.f);
	glScalef(0.33f, 0.25f, 1.f);
	glCallList(quad);
	glPopMatrix();

	// 위치
	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::EXTRA));
	glPushMatrix();
	glTranslatef(0.33f, 0.0f, 0.f);
	glScalef(0.33f, 0.25f, 1.f);
	glCallList(quad);
	glPopMatrix();

	// 입력
	if (g_tex != 0) {
		glBindTexture(GL_TEXTURE_2D, *tex);
		glPushMatrix();
		glTranslatef(0.0f, 0.25f, 0.f);
		glScalef(1.0f, 0.75f, 1.f);
		glCallList(quad);
		glPopMatrix();
	}
	
	// 깊이
	glBindTexture(GL_TEXTURE_2D, tracer->get_texture(PathTracer::DIFFUSE));
	glPushMatrix();
	glTranslatef(0.66f, 0.0f, 0.f);
	glScalef(0.34f, 0.25f, 1.f);
	glCallList(quad);
	glPopMatrix();
#endif
}

int main(int argc, char** argv)
{
	/* 창 초기화, 에러 핸들링 등록, 이벤트 콜백 등록, OpenGL 초기화 */
	/* -------------------------------------------------------------------------------------- */
	

	glfwInit();
	glfwSetErrorCallback([](int err, const char* desc) { puts(desc); });
	initContext(/*use dafault = */ true);
	//framebufferSizeCallback(nullptr, 512, 512);
	// 1 eye of resolution of HMD
	//framebufferSizeCallback(nullptr, 1080*1, 1200);
	if (argc > 1) {
		g_screenSize.cx = atoi(argv[1]);
		g_screenSize.cy = atoi(argv[2]);
		framebufferSizeCallback(nullptr, g_screenSize.cx, g_screenSize.cy);
	}
	else {
		framebufferSizeCallback(nullptr, 1024, 1024);
		//framebufferSizeCallback(nullptr, 1366, 768);
	}

	
	/*윈도우 제목창 없애기*/
	/*glfwWindowHint(GLFW_DECORATED, 0);*/
	GLFWwindow *window = glfwCreateWindow(g_screenSize.cx*g_viewScale, g_screenSize.cy * g_viewScale, "Foveated Rendering", nullptr, nullptr);

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetKeyCallback(window, keyboardCallback);
	

	glewInit();

	/* 객체 생성 */
	/* -------------------------------------------------------------------------------------- */
	PathTracer *tracer = new PathTracer();

	GBuffer* g_GBRenderer					= new GBuffer();
	ATrous * g_ATRenderer					= new ATrous();
	JumpFlooding * g_JFRenderer				= new JumpFlooding();
	PullPushInterpolation * g_PPIRenderer	= new PullPushInterpolation();
	SibsonInterpolation * g_SIRenderer		= new SibsonInterpolation();
	LogPolarTransform * g_LPRenderer		= new LogPolarTransform();

	Shader* g_normalShader = new Shader("./shader/nVS.glsl", "./shader/nFS.glsl");
	
	/* 초기화 */
	/* 성능측정 */
	g_fout.open("../Report/opti_performance_report.csv");
	/* -------------------------------------------------------------------------------------- */

	bool result = tracer->initialize(g_screenSize.cx, g_screenSize.cy);

	if (result) {
		puts("Optix Context 초기화!");
	}
	else {
		delete tracer;
		glfwTerminate();
		puts("Optix 초기화 실패!");
		return 0;
	}
	g_camera.setRotation(glm::quat());
	//g_camera.setPosition(glm::vec3(0.0f, 40.0f, 110.0f));

	// 복잡한 씬
	//g_camera.setPosition(glm::vec3(4.0f, 26.0f, 6.0f));

	// eye_exam
	 //g_camera.setPosition(glm::vec3(0, 200, 0)); g_camera.lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1));

	// vokselia_spawn
	g_camera.setPosition(glm::vec3(-2.12f, 1.53f, 2.11)); g_camera.lookAt(glm::vec3(0.0f));

	g_camera.setPosition(glm::vec3(-0.123401, 8.361134f, -0.223597)); g_camera.lookAt(glm::vec3(-3.992834f, -155.545700f, 6.774422f));
	
	// rungholt
	//g_camera.setPosition(glm::vec3(0.0f, 240.0f, 110.0f));
	// g_camera.setPosition(glm::vec3(-260, 205, 138)); g_camera.lookAt(glm::vec3(90, 20, 170));

	// lpshead
	//g_camera.setPosition(glm::vec3(0.0f, 0.0f, 1.0f));
	// lucy
	//g_camera.setPosition(glm::vec3(-1500, 1500, 1200.0f));

	//bunny
	//g_camera.setPosition(glm::vec3(+0.5f, 2.4f, 4.9f)); 	g_camera.lookAt(glm::vec3(-0.2f, 0.92f, 0.46));

	
	//g_camera.lookAt(glm::vec3(0.0f));
	g_camera.setProjectMode(Camera::PM_Perspective, 45, 0.1f, 500.1f);
	g_camera.setScreen(glm::vec2(g_screenSize.cx, g_screenSize.cy));
	g_camera.setViewport(glm::vec4(0, 0, g_screenSize.cx, g_screenSize.cy));

	tracer->init_camera(g_camera);
	puts("Optix Camera 설정!");

	// 성능측정 리포트
	char report[128] = { 0 };
	sprintf_s(report, "../Report/report.csv");
	fopen_s(&g_fp, report, "w");

	// 사진 이미지
	GLuint loadTex = loadBmp("../data/inpainting_test2.bmp", 4);

	
	/* 메인 루프 */
	/* -------------------------------------------------------------------------------------- */
	int save_count = 0;
	glGenQueries(sizeof(g_time_query) / sizeof(GLuint), g_time_query);
	while (!glfwWindowShouldClose(window))
	{
		GLuint64 elapsed_time = 0.0f; int done = 0; 
		clock_t total_t = 0;
		std::string text = "";
		int gl_query_idx = 0;

		// 이벤트 핸들링
		/* -------------------------------------------------------------------------------------- */
		glfwPollEvents();


		// GO TO
		/* -------------------------------------------------------------------------------------- */
		// 이벤트로 초기화
		if (g_shader_init) {
			g_GBRenderer->resetShader();
			g_JFRenderer->resetShader();
			g_SIRenderer->resetShader();
			g_LPRenderer->resetShader();
			g_PPIRenderer->resetShader();
			tracer->m_accumFrame = 0;

			g_shader_init = false;
		}

		tracer->update_optix_variables(g_camera);

		g_GBRenderer->render(&g_time_query[gl_query_idx], &elapsed_time, &done);
		gl_query_idx++;
		GLuint pt1 = g_GBRenderer->positionTex;
		GLuint nt1 = g_GBRenderer->normalTex;
		GLuint dt1 = g_GBRenderer->depthTex;
		PrintMSTimes("GB", text, elapsed_time, total_t);

		// GL -> optix
		//my_optix->map_gl_buffer("sampling_buffer", dt1);

		elapsed_time = tracer->geometry_launch();
		PrintMSTimes("Geometry", text, (elapsed_time) * 1E6, total_t);
		//my_optix->map_gl_buffer("position_buffer", pt1);

		elapsed_time = tracer->sampling_launch();
		PrintMSTimes("Sampling", text, (elapsed_time) * 1E6, total_t);

		elapsed_time = tracer->optimize_launch();
		PrintMSTimes("Optimize", text, (elapsed_time) * 1E6, total_t);

		elapsed_time = tracer->shading_launch();
		PrintMSTimes("Shading", text, (elapsed_time) * 1E6, total_t);

		{
			void* data;
			auto gaze_target_buffer = tracer->m_context["gaze_target"]->getBuffer()->get();
			rtBufferMap(gaze_target_buffer, &data);
			RTsize w = 1;
			rtBufferGetSize1D(gaze_target_buffer, &w);
			glm::vec3 gaze_target = glm::vec3(((float*)data)[0], ((float*)data)[1], ((float*)data)[2]);
			rtBufferUnmap(gaze_target_buffer);
			g_camera.setTarget(gaze_target);
		}
		{
			void* data;
			auto buffer = tracer->m_context["ray_count"]->getBuffer()->get();
			rtBufferMap(buffer, &data);
			RTsize w = 1;
			rtBufferGetSize1D(buffer, &w);
			int trace_sample = ((unsigned int*)data)[0];
			rtBufferUnmap(buffer);
			text += "ray count, " + std::to_string(trace_sample) + ", ";
			float trace_sample_percent = (float)trace_sample / (g_screenSize.cx*g_screenSize.cy) * 100.0f;
			text += "(%), " + std::to_string(trace_sample_percent) + ", ";
		}
		

		

		// 성능측정
		//static int count_k = 0; static float trace_speed_acc = 0.0f;
		//if (my_optix->m_accumFrame > 5) {
		//	if (g_apectureSize >= 0.0f) {
		//		if (count_k != 0 && count_k % 10 == 0) {
		//			g_fout << "number of ray, " << trace_sample << ", " << trace_speed_acc / 10.0f << std::endl;
		//			//printf("aspecture: %f, rendering time: %f\n", g_apectureSize, trace_speed_acc);
		//			g_apectureSize -= 0.01f;
		//			trace_speed_acc = 0.0f;
		//		}
		//		else {
		//			trace_speed_acc += elapsed_time;
		//		}
		//	}
		//	else {
		//		g_fout.close();
		//		exit(0);
		//	}
		//	count_k++;
		//}

		GLuint pos_t = tracer->get_texture(PathTracer::POSITION);
		GLuint norm_t = tracer->get_texture(PathTracer::NORMAL);
		GLuint depth_t = tracer->get_texture(PathTracer::DEPTH);
		GLuint diffuse_t = tracer->get_texture(PathTracer::DIFFUSE);
		GLuint weight_tex = tracer->get_texture(PathTracer::WEIGHT);
		GLuint thread_tex = tracer->get_texture(PathTracer::THREAD);
		GLuint history_tex = tracer->get_texture(PathTracer::HISTORY);
		GLuint shading_tex = tracer->get_texture(PathTracer::SHADING);
		GLuint extra_tex = tracer->get_texture(PathTracer::EXTRA);
		
		// Jump Flooding
		g_JFRenderer->render(shading_tex, &g_time_query[gl_query_idx], &elapsed_time, &done);
		gl_query_idx++;
		GLuint jfa_tex = g_JFRenderer->colorTex;
		GLuint jfa_coord_tex = g_JFRenderer->coordTex;
		PrintMSTimes("JPA", text, elapsed_time, total_t);

		// Sibson's Interpolation
		g_SIRenderer->render(jfa_coord_tex, jfa_tex, &g_time_query[gl_query_idx], &elapsed_time, &done);
		gl_query_idx++;
		GLuint si_tex = g_SIRenderer->outputTex;
		PrintMSTimes("SI", text, elapsed_time, total_t);

		// Pull-Push
		g_PPIRenderer->render(shading_tex, &g_time_query[gl_query_idx], &elapsed_time, &done);
		gl_query_idx++;
		GLuint ppi_tex = g_PPIRenderer->outputTex;
		PrintMSTimes("PPI", text, elapsed_time, total_t);

		// A-trous
		g_ATRenderer->render(1, pos_t, norm_t, ppi_tex, jfa_tex, tracer->m_accumFrame, &g_time_query[gl_query_idx], &elapsed_time, &done);
		gl_query_idx++;
		GLuint atrous_tex = g_ATRenderer->colorTex;
		PrintMSTimes("AT", text, elapsed_time, total_t);

		//// Log-Polar
		//g_LPRenderer->render(ppit, &g_time_query[gl_query_idx], &elapsed_time, &done);
		//gl_query_idx++;
		//GLuint lpt = g_LPRenderer->logPolarTex;
		//GLuint ilpt = g_LPRenderer->ilogPolarTex;
		//PrintMSTimes("LP", text, elapsed_time, total_t);


		total_t += (elapsed_time / 1e6);
		g_FPS = 1.0f / round(1E3 / total_t);

		text += "display," + std::to_string(elapsed_time / 1e6) + ", ";
		text += "Total," + std::to_string(total_t) + ", ";
		text += "FPS," + std::to_string(round(1E3 / total_t)) + ", ";
		text += "apercture, " + std::to_string(g_apertureSize) + ", " + "g_number_of_ray, " + std::to_string(g_number_of_ray) + "\n";
		//std::cerr << text;

		// 성능측정
		{
			/*if (my_optix->m_accumFrame > 2 && my_optix->m_accumFrame < 300 + 2)
			{
				fprintf_s(g_fp, "%s", text.c_str());

			}
			if (my_optix->m_accumFrame > 300 + 2) {
				fclose(g_fp);
				exit(0);
			}*/
		}
// 		text = std::to_string(my_optix->m_accumFrame) + "\n";
// 		std::cerr << text;

		//if (g_tex != nullptr)
		//	my_optix->render(*g_tex);
		
		// render normally
		if (g_isTextureChanged > 0) {
			switch (g_isTextureChanged) {
			case 1: g_tex = &depth_t; break;
			case 2: g_tex = &diffuse_t; break;
			case 3: g_tex = &weight_tex; break;
			case 4: g_tex = &jfa_tex; break;
			case 5: g_tex = &si_tex; break;
			case 6: g_tex = &atrous_tex; break;
			case 7: g_tex = &ppi_tex; break;
			case 8: g_tex = &shading_tex; break;
			case 9: g_tex = &extra_tex; break;
			}
			g_isTextureChanged = 0;
		}

		glBeginQuery(GL_TIME_ELAPSED, g_time_query[gl_query_idx]);

		if (g_fullScreen) {
			g_normalShader->use();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, 1, 0, 1, -1, 1);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glViewport(0, 0, g_screenSize.cx * g_viewScale, g_screenSize.cy * g_viewScale);

			glActiveTexture(GL_TEXTURE0);
			if (g_tex != 0)
				glBindTexture(GL_TEXTURE_2D, *g_tex);
			glUniform2f(0, (float)g_screenSize.cx * g_viewScale, (float)g_screenSize.cy * g_viewScale);

			glDrawArrays(GL_QUADS, 0, 4);
			glBindTexture(GL_TEXTURE_2D, 0);
			g_normalShader->unuse();
		}
		else {
			renderAll(tracer, g_tex);
		}


		glEndQuery(GL_TIME_ELAPSED);
		glGetQueryObjectiv(g_time_query[gl_query_idx], GL_QUERY_RESULT_AVAILABLE, &done);
		glGetQueryObjectui64v(g_time_query[gl_query_idx], GL_QUERY_RESULT, &elapsed_time);

		// 스왑 버퍼
		/* -------------------------------------------------------------------------------------- */
		glfwSwapBuffers(window);
		g_camera.setPrevState();

		if (g_saveBMP) {
			char fileName[128] = { 0 };
			static int imgCount = 1;
			sprintf_s(fileName, "%s_%d", "save", imgCount);
			sprintf_s(fileName, "%s_%.2f_%d", "save", g_apertureSize, imgCount);
			saveBMP24(fileName);

			/*glm::vec3 target = g_camera.getTarget();
			static float radian = 1.0f;
			g_camera.rotateAround(target, sin(radian) * 0.1f, g_camera.getRight());
			radian += 1.0f;
			g_camera_moved = true;

			if (imgCount > 12)*/
				g_saveBMP = false;
			imgCount++;
		}
	}
	glDeleteQueries(sizeof(g_time_query) / sizeof(GLuint), g_time_query);

	// 객체 제거
	/* -------------------------------------------------------------------------------------- */

	delete g_GBRenderer;
	delete g_ATRenderer;
	delete g_JFRenderer;
	delete g_PPIRenderer;
	delete g_SIRenderer;
	delete g_LPRenderer;
	delete g_normalShader;
	delete tracer;


	// 종료
	/* -------------------------------------------------------------------------------------- */
	glfwTerminate();
}