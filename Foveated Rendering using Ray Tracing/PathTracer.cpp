// gl + gui
#include <gl/glew.h>
#include "gui.h"

#include "PathTracer.h"
#include "./cuda/device_include/commonStructs.h"

#include <cuda.h>
// optix
#include <sutil.h>
#include <optix_math.h>
#include <optixu/optixu_aabb.h>
#include <optixu/optixu_math_stream.h>
#include <OptiXMesh.h>
#include <Camera.h>

// std
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <time.h>

// 함수 선언
/**********************************************************************/
std::string load_file(const char* filepath);
optix::TextureSampler createTextureSamplerFromBuffer(optix::Context & context, optix::Buffer buffer);

// 메소드 구현
/**********************************************************************/
PathTracer::~PathTracer()
{
	if (m_is_valid)
		m_context->destroy();

	m_is_valid = false;

	glDeleteTextures(sizeof(m_textures)/sizeof(GLuint), m_textures);
}

bool PathTracer::initialize(int width, int height)
{
	if (m_is_valid) {
		return false;
	}

	m_width = width;
	m_height = height;

	// Optix
	/************************************************************************/
	init_context();
	init_program();
	init_buffers();
	init_geometry();
	m_context->validate();
	init_camera(g_camera);
	//init_camera(optix::make_float3(0, 0, 5), optix::make_float3(0, 1, 0), optix::make_float3(0, 0, 0));

	// OpenGL
	/************************************************************************/
	glGenTextures(sizeof(m_textures) / sizeof(GLuint), m_textures);
	for (auto& id : m_textures) {
		glBindTexture(GL_TEXTURE_2D, id);

		// Change these to GL_LINEAR for super- or sub-sampling
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		// GL_CLAMP_TO_EDGE for linear filtering, not relevant for nearest.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	m_is_valid = true;

	return m_is_valid;
}

static __inline__ optix::float3 logf(optix::float3 v)
{
	return optix::make_float3(logf(v.x), logf(v.y), logf(v.z));
}

float PathTracer::geometry_launch()
{
	using namespace optix;

	clock_t start, end;

	static float3 glass_transmittance = make_float3(0.1f, 0.63f, 0.3f);
	static float log_transmittance_depth = 0.0f;
	static int max_depth = 100;
	const float t0 = expf(log_transmittance_depth);
	const float3 extinction = -logf(glass_transmittance) / t0;
	m_context["extinction"]->setFloat(extinction);
	m_context["max_depth"]->setInt(max_depth);
	m_context["top_object"]->set(m_top_group);
	m_context["frame"]->setUint(m_accumFrame++);
	m_context["g_apertureSize"]->setFloat(g_apertureSize);

	// 카메라가 움직이거나, 이벤트로 인한 업데이트 발생시
	if (g_light_changed) {
		ParallelogramLight light;
		light.corner = make_float3(343.0f, 548.6f, 227.0f);
		light.v1 = make_float3(-130.0f, 0.0f, 0.0f);
		light.v2 = make_float3(0.0f, 0.0f, 105.0f);
		light.normal = normalize(cross(light.v1, light.v2));
		light.emission = make_float3(g_light_Power);
		memcpy(m_light_buffer->map(), &light, sizeof(light));
		m_light_buffer->unmap();
		m_context["lights"]->setBuffer(m_light_buffer);

		g_light_changed = false;
		m_accumFrame = 0;
	}
	

	start = clock();
	m_context->launch(0, m_width, m_height);
	{
		// fold 3times
		/*for (int i = 0; i < 3; i++) {
			m_context["step"]->setUint(i);
			m_context->launch(1, m_width / 2, m_height / 2);
		}*/

		// CSR format comppress
		//m_context["step"]->setUint(0);
		//m_context->launch(2, 1, m_height);

		/*m_context["step"]->setUint(1);
		m_context->launch(1, 1, m_height / 2);

		m_context["step"]->setUint(2);
		m_context->launch(1, m_width, 1);*/

		// nearest block unit compress
		/*for (int i = 0; i < 1; i++) {
			m_context["step"]->setUint(i);
			m_context->launch(1, m_width / 2, m_height / 2);
		}*/

// 		m_context["step"]->setUint(1);
// 		m_context["divide"]->setUint(2);
// 		m_context->launch(1, 1, m_height / 2);

		//m_context["step"]->setUint(2);
		//m_context["divide"]->setUint(4);
		//m_context->launch(2, 1, m_height / 4);

  		/*m_context["step"]->setUint(8);
  		m_context["divide"]->setUint(16);
  		m_context->launch(2, 1, m_height / 16);*/

		// 8*4 block compress
// 		m_context["step"]->setUint(8);
// 		m_context["divide"]->setUint(16);
// 		m_context->launch(1, 1, 1);
	}
	end = clock();

	return end - start;
}

float PathTracer::sampling_launch()
{
	clock_t start, end;
	start = clock();
	m_context->launch(1, m_width, m_height);
	end = clock();
	return end - start;
}

float PathTracer::optimize_launch()
{
	clock_t start, end;
	start = clock();

#if 1
	/*** 최적화 (가로 -> 세로) ***/
	if (g_isOptimize) {
		m_context["step"]->setUint(0);
		m_context->launch(2, 1, m_height);

		m_context["step"]->setUint(31);
		m_context->launch(2, m_width, 1);

		// 레이트래이스 쓰레드 카운팅
		m_context["step"]->setUint(30);
		m_context->launch(2, 1, 1);
	}
#else
	/*** 최적화 (세로 -> 가로 -> 끝 조절) ***/
	if (g_isOptimize) {
		swapBuffer("thread_buffer", "thread_cache");
		m_context["step"]->setUint(31);
		m_context->launch(2, m_width, 1);

		m_context["step"]->setUint(0);
		m_context->launch(2, 1, m_height);

		// 레이트래이스 쓰레드 카운팅
		m_context["step"]->setUint(30);
		m_context->launch(2, 1, 1);
		swapBuffer("thread_buffer", "thread_cache");

		m_context["step"]->setUint(5);
		m_context["divide"]->setUint(2);
		m_context->launch(2, 1, m_height / 2);
	}
#endif

	end = clock();
	return end - start;
}

float PathTracer::shading_launch()
{
	clock_t start, end;
	start = clock();
	m_context->launch(3, m_width, m_height);
	end = clock();

	// swap Buffer
	swapBuffer("history_cache", "history_buffer");
	swapBuffer("depth_cache", "depth_buffer");
	
	return end - start;
}

void PathTracer::swapBuffer(const char* a_buffer, const char* b_buffer) {
	const optix::Buffer A = m_context[a_buffer]->getBuffer();
	const optix::Buffer B = m_context[b_buffer]->getBuffer();

	m_context[a_buffer]->set(B);
	m_context[b_buffer]->set(A);
}

void PathTracer::update_textures()
{
	update_gl_texture("position_buffer", get_texture(POSITION));
	update_gl_texture("normal_buffer", get_texture(NORMAL));
	update_gl_texture("depth_buffer", get_texture(DEPTH));
	update_gl_texture("diffuse_buffer", get_texture(DIFFUSE));
	update_gl_texture("weight_buffer", get_texture(WEIGHT));
	update_gl_texture("thread_buffer", get_texture(THREAD));
	update_gl_texture("history_buffer", get_texture(HISTORY));
	update_gl_texture("shading_buffer", get_texture(SHADING));
	update_gl_texture("extra_buffer", get_texture(EXTRA));
}

bool PathTracer::update_gl_texture(const char* buffer_name, GLuint& opengl_texture_id)
{
	optix::Buffer buffer = m_context[buffer_name] ?
		m_context[buffer_name]->getBuffer() : nullptr;
	
	if (!buffer) {
		printf_s("%s 라는 버퍼는 없습니다.\n", buffer_name, strlen(buffer_name));
		return 0;
	}

	// Query buffer information
	RTsize buffer_width_rts, buffer_height_rts;
	buffer->getSize(buffer_width_rts, buffer_height_rts);
	uint32_t width = static_cast<int>(buffer_width_rts);
	uint32_t height = static_cast<int>(buffer_height_rts);
	RTformat buffer_format = buffer->getFormat();

	// Check if we have a GL interop display buffer
	const unsigned pboId = buffer->getGLBOId();
	if (pboId)
	{
		glBindTexture(GL_TEXTURE_2D, opengl_texture_id);

		// send PBO to texture
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboId);

		RTsize elmt_size = buffer->getElementSize();
		if (elmt_size % 8 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
		else if (elmt_size % 4 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		else if (elmt_size % 2 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		else                          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		switch (buffer_format) {
		case RT_FORMAT_UNSIGNED_BYTE4: 
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0); 
			break;
		case RT_FORMAT_FLOAT4:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, width, height, 0, GL_RGBA, GL_FLOAT, 0);
			break;
		case RT_FORMAT_FLOAT3:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB, width, height, 0, GL_RGB, GL_FLOAT, 0);
			break;
		case RT_FORMAT_FLOAT:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE32F_ARB, width, height, 0, GL_LUMINANCE, GL_FLOAT, 0);
			break;
		}

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	return true;
}

void PathTracer::render(int texNum)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, g_screenSize.cx, g_screenSize.cy);

	glBindTexture(GL_TEXTURE_2D, get_texture(static_cast<TextureName>(texNum)));
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, 1.0f);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint& PathTracer::get_texture(TextureName name)
{
	switch (name)
	{
	case PathTracer::POSITION:
		update_gl_texture("position_buffer", m_textures[POSITION]);
		return m_textures[POSITION];
	case PathTracer::NORMAL:
		update_gl_texture("normal_buffer", m_textures[NORMAL]);
		return m_textures[NORMAL];
	case PathTracer::DEPTH:
		update_gl_texture("depth_buffer", m_textures[DEPTH]);
		return m_textures[DEPTH];
	case PathTracer::DIFFUSE:
		update_gl_texture("diffuse_buffer", m_textures[DIFFUSE]);
		return m_textures[DIFFUSE];
	case PathTracer::WEIGHT:
		update_gl_texture("weight_buffer", m_textures[WEIGHT]);
		return m_textures[WEIGHT];
	case PathTracer::THREAD:
		update_gl_texture("thread_buffer", m_textures[THREAD]);
		return m_textures[THREAD];
	case PathTracer::HISTORY:
		update_gl_texture("history_buffer", m_textures[HISTORY]);
		return m_textures[HISTORY];
	case PathTracer::SHADING:
		update_gl_texture("shading_buffer", m_textures[SHADING]);
		return m_textures[SHADING];
	case PathTracer::EXTRA:
		update_gl_texture("extra_buffer", m_textures[EXTRA]);
		return m_textures[EXTRA];
	
	
	
	}

	return m_textures[0];
}

const GLuint& PathTracer::get_texture(TextureName name) const
{
	switch (name)
	{
	case PathTracer::POSITION:
		return m_textures[POSITION];
	case PathTracer::NORMAL:
		return m_textures[NORMAL];
	case PathTracer::DEPTH:
		return m_textures[DEPTH];
	case PathTracer::DIFFUSE:
		return m_textures[DIFFUSE];
	case PathTracer::WEIGHT:
		return m_textures[WEIGHT];
	case PathTracer::THREAD:
		return m_textures[THREAD];
	case PathTracer::HISTORY:
		return m_textures[HISTORY];
	case PathTracer::SHADING:
		return m_textures[SHADING];
	case PathTracer::EXTRA:
		return m_textures[EXTRA];
	}

	return m_textures[0];
}

void PathTracer::init_context()
{
	m_accumFrame = 0;

	using namespace optix;

	m_context = optix::Context::create();
	m_context->setRayTypeCount(3); // [0] G-buffer, [1] default ray, [2] to light ray
	m_context->setEntryPointCount(4); // [0] G-buffer, [1] Sampling,  [2] Thread Optimization, [3] Path-Trace

	m_context->setStackSize(1800);
}

void PathTracer::init_program() 
{
	using namespace optix;
	// G-Buffer program
	std::string ptx_path = load_file("cuda/g_buffer_trace_camera.ptx");
	Program g_buffer_gen_program = m_context->createProgramFromPTXString(ptx_path, "g_buffer_trace");
	m_context->setRayGenerationProgram(0, g_buffer_gen_program);

	{
		// Sampling Step
		ptx_path = load_file("cuda/samplingStep.ptx");
		Program sampling_program = m_context->createProgramFromPTXString(ptx_path, "sampling_step");
		m_context->setRayGenerationProgram(1, sampling_program);

		// Optimiza Step
		ptx_path = load_file("cuda/warpSort.ptx");
		Program sort_program = m_context->createProgramFromPTXString(ptx_path, "warp_sort");
		m_context->setRayGenerationProgram(2, sort_program);
	}

	// Ray generation program
	ptx_path = load_file("cuda/fov_path_trace_camera.ptx");
	Program ray_gen_program = m_context->createProgramFromPTXString(ptx_path, "ray_trace");
	m_context->setRayGenerationProgram(3, ray_gen_program);

	// Exception program
	Program exception_program = m_context->createProgramFromPTXString(ptx_path, "exception");
	m_context->setExceptionProgram(3, exception_program);
	m_context["bad_color"]->setFloat(1.0f, 1.0f, 1.0f, 0.0f);

	// Miss program
	ptx_path = load_file("cuda/gradientbg.ptx");
	//m_context->setMissProgram(0, m_context->createProgramFromPTXString(ptx_path, "g_miss"));
	//m_context->setMissProgram(1, m_context->createProgramFromPTXString(ptx_path, "miss"));

	const float3 default_color = make_float3(1.0f, 1.0f, 1.0f);
	m_context->setMissProgram(0, m_context->createProgramFromPTXString(ptx_path, "g_miss"));
	m_context->setMissProgram(1, m_context->createProgramFromPTXString(ptx_path, "envmap_miss"));
	const std::string texpath = "../resource/" + std::string("CedarCity.hdr");
	m_context["envmap"]->setTextureSampler(sutil::loadTexture(m_context, texpath, default_color));
	m_context["bg_color"]->setFloat(make_float3(0.34f, 0.55f, 0.85f));
}

void PathTracer::init_buffers() 
{
	using namespace optix;

	Buffer ray_count = m_context->createBuffer(RT_BUFFER_OUTPUT);
	ray_count->setFormat(RT_FORMAT_UNSIGNED_INT);
	ray_count->setSize(1u);
	unsigned int k = 0;
	memcpy(ray_count->map(), &k, sizeof(unsigned int));
	ray_count->unmap();
	m_context["ray_count"]->setBuffer(ray_count);

	m_context["max_depth"]->setInt(1);
	m_context["cutoff_color"]->setFloat(0.2f, 0.2f, 0.2f);
	m_context["frame"]->setUint(m_accumFrame);
	m_context["scene_epsilon"]->setFloat(1.e-3);

	// Color Buffer
	Buffer shading_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["shading_buffer"]->set(shading_buffer);

	Buffer normal_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["normal_buffer"]->set(normal_buffer);

	Buffer position_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["position_buffer"]->set(position_buffer);

	Buffer diffuse_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["diffuse_buffer"]->set(diffuse_buffer);

	Buffer depth_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["depth_buffer"]->set(depth_buffer);

	Buffer depth_cache = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["depth_cache"]->set(depth_cache);

	Buffer weight_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["weight_buffer"]->set(weight_buffer);

	// Accumulation buffer - 임시 (나중에 아래로 변경)
	Buffer extra_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["extra_buffer"]->set(extra_buffer);

	/*Buffer extra_buffer = m_context->createBuffer(RT_BUFFER_INPUT_OUTPUT | RT_BUFFER_GPU_LOCAL,
	RT_FORMAT_FLOAT4, m_width, m_height);
	m_context["extra_buffer"]->set(extra_buffer);*/

	// for history
	Buffer history_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["history_buffer"]->set(history_buffer);
	// cache_buffer for history
	Buffer history_cache = sutil::createOutputBuffer(m_context, RT_FORMAT_FLOAT4, m_width, m_height, true);
	m_context["history_cache"]->set(history_cache);

	Buffer thread_buffer = sutil::createOutputBuffer(m_context, RT_FORMAT_UNSIGNED_INT3, m_width, m_height, true);
	m_context["thread_buffer"]->set(thread_buffer);

	Buffer thread_cache = sutil::createOutputBuffer(m_context, RT_FORMAT_UNSIGNED_INT3, m_width, m_height, true);
	m_context["thread_cache"]->set(thread_cache);

	// 샘플링 맵을 위한 버퍼 정의 (반드시 RT_BUFFER_INPUT 으로만 설정가능)
	{
		Buffer sampling_buffer = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, m_width, m_height);
		m_context["sampling_buffer"]->set(sampling_buffer);
		
		// 샘플링 맵 초기화 모두 1로 세팅 (full resolution)
		auto b_data = (float4*)sampling_buffer->map();
		for (int h = 0; h < m_height; h++) {
			for (int w = 0; w < m_width; w++) {
				if (w % 2 == 0 && h % 2 == 0)
					b_data[h*m_width + w] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);// = 1.0f;
				else
					b_data[h*m_width + w] = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
			}
		}
		//memset(b_data, 0x00, m_width * m_height * sizeof(float));
		sampling_buffer->unmap();
	}
	

	m_context["background_light"]->setFloat(1.0f, 1.0f, 1.0f);
	m_context["background_dark"]->setFloat(0.3f, 0.3f, 0.3f);

	// align background's up direction with camera's look direction
	glm::vec3 cam_up =  g_camera.getUp();
	float3 bg_up = normalize(make_float3(cam_up.x, cam_up.y, cam_up.z));
	m_context["up"]->setFloat(bg_up.x, bg_up.y, bg_up.z);


	Buffer gaze_target = m_context->createBuffer(RT_BUFFER_OUTPUT);
	gaze_target->setFormat(RT_FORMAT_FLOAT3);
	gaze_target->setSize(1u);
	glm::vec3 target = g_camera.getTarget();
	memcpy(gaze_target->map(), &target, sizeof(glm::vec3));
	gaze_target->unmap();
	m_context["gaze_target"]->setBuffer(gaze_target);

	m_context["cam_target"]->setFloat(target.x, target.y, target.z);
}

void PathTracer::init_geometry()
{
	using namespace optix;

	// Light buffer
	ParallelogramLight light;
	light.corner = make_float3(343.0f, 548.6f, 227.0f);
	light.v1 = make_float3(-130.0f, 0.0f, 0.0f);
	light.v2 = make_float3(0.0f, 0.0f, 105.0f);
	light.normal = normalize(cross(light.v1, light.v2));
	light.emission = make_float3(g_light_Power);

	m_light_buffer = m_context->createBuffer(RT_BUFFER_INPUT);
	m_light_buffer->setFormat(RT_FORMAT_USER);
	m_light_buffer->setElementSize(sizeof(ParallelogramLight));
	m_light_buffer->setSize(1u);
	memcpy(m_light_buffer->map(), &light, sizeof(light));
	m_light_buffer->unmap();
	m_context["lights"]->setBuffer(m_light_buffer);
	
	m_context["light_position"]->setFloat(make_float3(343.0f, 548.6f, 227.0f));


	m_model_info.push_back({ "../resource/ground.obj", "../resource/grid.ppm", 
		Matrix4x4::translate(make_float3(0.0f, -0.05f, 0.0f)), MaterialType::MATL_DIFFUSE });
	m_model_info.push_back({ "../resource/vokselia_spawn/vokselia_spawn.obj", "../resource/vokselia_spawn/vokselia_spawn.ppm",
		Matrix4x4::identity(), MaterialType::MATL_DIFFUSE });
	Matrix4x4 model_2 = Matrix4x4::translate(make_float3(-3.5f, 0.2f, 1.2f)) * Matrix4x4::scale(make_float3(0.01f));
	m_model_info.push_back({ "../resource/box/box.obj", "../resource/grid.ppm",
		model_2, MaterialType::MATL_REFRACTION });
	Matrix4x4 model_1 = Matrix4x4::translate(make_float3(-1.5f, 0.2f, 1.2f)) * Matrix4x4::scale(make_float3(0.25f));
	m_model_info.push_back({ "../resource/bunny/bunny.obj", "../resource/bunny/bunny.ppm",
		model_1, MaterialType::MATL_REFRACTION });

	m_model_info.push_back({ "../resource/earth/earth.obj", "none",
		Matrix4x4::translate(make_float3(0.0f, 1.0f, 0.0f)) * Matrix4x4::scale(make_float3(0.01f)),
			MaterialType::MATL_REFLECTION });

	//m_model_info.push_back({ "../resource/bunny/bunny.obj", optix::Matrix4x4::translate(make_float3(-150.0f, 1.0f, 120.0f)), MaterialType::MATL_REFRACTION });

	m_aabb = createGeometry(m_model_info);

	m_context["bbox_min"]->setFloat(m_aabb.m_min);
	m_context["bbox_max"]->setFloat(m_aabb.m_max);
}


void PathTracer::init_camera(const Camera& camera)
{
	if (g_camera_moved) {
		//m_accumFrame = 0;
		g_camera_moved = false;
	}
	
	glm::mat4 prevMVP = camera.getPrevMVP();
	glm::vec3 pos = camera.getPosition();
	glm::mat4 mat = camera.getPMat() * camera.getVMat();

	m_context["eye"]->setFloat(pos.x, pos.y, pos.z);
	optix::Matrix4x4 optix_MVP = optix::Matrix4x4(&mat[0][0]);
	m_context["mvp"]->setMatrix4x4fv(true, optix_MVP.inverse().getData());

// 	float eye_variance = glm::abs(glm::dot(pos + camera.getFront(), (camera.getPrevPos() - pos)));
// 	m_context["eye_variance"]->setFloat(eye_variance);
// 	printf("%f eye_variance\n", eye_variance);

	optix::Matrix4x4 optix_prevMVP = optix::Matrix4x4(&prevMVP[0][0]);
	m_context["prev_mvp"]->setMatrix4x4fv(true, optix_prevMVP.getData());

	glm::vec3 prevPos = camera.getPrevPos();
	m_context["prev_eye"]->setFloat(prevPos.x, prevPos.y, prevPos.z);

	m_context["gaze"]->setFloat(float(g_screenSize.cx * 0.5f), float(g_screenSize.cy - g_screenSize.cy * 0.5f));
}

optix::Aabb PathTracer::createGeometry(std::vector<Model>& models)
{
	using namespace optix;

	const std::string ptx_path = load_file("./cuda/triangle_mesh.ptx");

	m_top_group = m_context->createGroup();
	m_top_group->setAcceleration(m_context->createAcceleration("Trbvh"));

	int num_triangles = 0;

	optix::Aabb aabb;
	{
		GeometryGroup geometry_group = m_context->createGeometryGroup();
		geometry_group->setAcceleration(m_context->createAcceleration("Trbvh"));
		m_top_group->addChild(geometry_group);
		for (size_t i = 0; i < models.size(); ++i) {

			OptiXMesh mesh;
			mesh.context = m_context;

			// override defaults
			mesh.intersection = m_context->createProgramFromPTXString(ptx_path, "mesh_intersect_refine");
			mesh.bounds = m_context->createProgramFromPTXString(ptx_path, "mesh_bounds");
			mesh.material = load_obj(models[i]);

			loadMesh(models[i].m_mesh_filename, mesh, models[i].m_mesh_xforms);
			geometry_group->addChild(mesh.geom_instance);

			aabb.include(mesh.bbox_min, mesh.bbox_max);

			std::cerr << models[i].m_mesh_filename << ": " << mesh.num_triangles << std::endl;
			num_triangles += mesh.num_triangles;
		}
		std::cerr << "Total triangle count: " << num_triangles << std::endl;
	}

	m_context["top_object"]->set(m_top_group);

	return aabb;
}

optix::Material PathTracer::load_obj(Model& model)
{
	using namespace optix;
	optix::Material material = 0;

	optix::Program gd_program = m_context->createProgramFromPTXFile(
		"./cuda/g_diffuse.ptx",
		"diffuse");

	optix::Program material_program = 0;
	optix::Program shadow_program = 0;

	switch (model.m_matl_type) {
	case MaterialType::MATL_DIFFUSE:
		material_program = m_context->createProgramFromPTXFile(
			"./cuda/diffuse.ptx",
			"diffuse");
		shadow_program = m_context->createProgramFromPTXFile(
			"./cuda/diffuse.ptx",
			"shadow");

		material = m_context->createMaterial();
		material->setClosestHitProgram(0, gd_program);
		material->setClosestHitProgram(1, material_program);
		material->setAnyHitProgram(2, shadow_program);

		material["Kd_map"]->setTextureSampler(sutil::loadTexture(m_context, model.m_texture_name, optix::make_float3(1.0f)));
		material["Kd_map_scale"]->setFloat(make_float2(1.0f, 1.0f));
		material["diffuse_max_depth"]->setInt(4);

		material["screen"]->setFloat(make_float2(g_screenSize.cx, g_screenSize.cy));
		break;

	case MaterialType::MATL_REFLECTION:
		material_program = m_context->createProgramFromPTXFile(
			"./cuda/reflection.ptx",
			"reflection");
		shadow_program = m_context->createProgramFromPTXFile(
			"./cuda/reflection.ptx",
			"shadow");

		material = m_context->createMaterial();
		material->setClosestHitProgram(0, gd_program);
		material->setClosestHitProgram(1, material_program);
		material->setAnyHitProgram(2, shadow_program);

		material["Kd_map"]->setTextureSampler(sutil::loadTexture(m_context, model.m_texture_name, optix::make_float3(1.0f)));
		material["Kd_map_scale"]->setFloat(make_float2(1.0f, 1.0f));
		material["reflection_max_depth"]->setInt(4);

		//material["Ka"]->setFloat(0.3f, 0.3f, 0.1f);
		//material["Kd"]->setFloat(194 / 255.f*.6f, 186 / 255.f*.6f, 151 / 255.f*.6f);
		material["Ks"]->setFloat(1.0f, 1.0f, 1.0f);
		//material["reflectivity"]->setFloat(0.1f, 0.1f, 0.1f);
		material["reflectivity_n"]->setFloat(0.05f, 0.05f, 0.05f);
		material["phong_exp"]->setFloat(88);
		material["importance_cutoff"]->setFloat(1e-2f);

		material["screen"]->setFloat(make_float2(g_screenSize.cx, g_screenSize.cy));
		break;

	case MaterialType::MATL_REFRACTION:
		material_program = m_context->createProgramFromPTXFile("./cuda/refraction.ptx",
			"refraction");
		shadow_program = m_context->createProgramFromPTXFile("./cuda/refraction.ptx",
			"any_hit_shadow");

		material = m_context->createMaterial();
		material->setClosestHitProgram(0, gd_program);
		material->setClosestHitProgram(1, material_program);
		material->setAnyHitProgram(2, shadow_program);

		material["importance_cutoff"]->setFloat(1e-2f);
		material["cutoff_color"]->setFloat(0.34f, 0.55f, 0.85f);
		material["fresnel_exponent"]->setFloat(3.0f);
		material["fresnel_minimum"]->setFloat(0.1f);
		material["fresnel_maximum"]->setFloat(1.0f);
		material["refraction_index"]->setFloat(1.4f);
		material["refraction_color"]->setFloat(1.0f, 1.0f, 1.0f);
		material["reflection_color"]->setFloat(1.0f, 1.0f, 1.0f);
		material["refraction_maxdepth"]->setInt(100);
		material["reflection_maxdepth"]->setInt(100);
		float3 extinction = make_float3(1.0f, 1.0f, 1.0f);
		material["extinction_constant"]->setFloat(log(extinction.x), log(extinction.y), log(extinction.z));
		material["shadow_attenuation"]->setFloat(1.0f, 1.0f, 1.0f);

		material["Kd_map"]->setTextureSampler(sutil::loadTexture(m_context, model.m_texture_name, optix::make_float3(1.0f)));
		material["Kd_map_scale"]->setFloat(make_float2(1.0f, 1.0f));

		material["screen"]->setFloat(make_float2(g_screenSize.cx, g_screenSize.cy));
		break;
	}

	model.m_material = material;

	return material;
}

void PathTracer::update_optix_variables(const Camera& camera)
{
	if (g_camera_moved) {
		//m_accumFrame = 0;
		g_camera_moved = false;
	}

	glm::mat4 prevMVP = camera.getPrevMVP();
	glm::vec3 pos = camera.getPosition();
	glm::mat4 mat = camera.getPMat() * camera.getVMat();

	m_context["eye"]->setFloat(pos.x, pos.y, pos.z);
	optix::Matrix4x4 optix_MVP = optix::Matrix4x4(&mat[0][0]);
	m_context["mvp"]->setMatrix4x4fv(true, optix_MVP.inverse().getData());

	optix::Matrix4x4 optix_prevMVP = optix::Matrix4x4(&prevMVP[0][0]);
	m_context["prev_mvp"]->setMatrix4x4fv(true, optix_prevMVP.getData());

	glm::vec3 prevPos = camera.getPrevPos();
	m_context["prev_eye"]->setFloat(prevPos.x, prevPos.y, prevPos.z);

	m_context["gaze"]->setFloat(float(g_gaze.x), float(g_screenSize.cy - g_gaze.y));
	
	glm::vec3 cam_up = g_camera.getUp();
	m_context["up"]->setFloat(cam_up.x, cam_up.y, cam_up.z);

	for (int i = 0; i < m_model_info.size(); i++) {
		switch (m_model_info[i].m_matl_type) {
		case MaterialType::MATL_DIFFUSE:
			m_model_info[i].m_material["diffuse_max_depth"]->setInt(g_diffuse_max_depth);
			break;
		}
	}

	glm::vec3 target = g_camera.getTarget();
	m_context["cam_target"]->setFloat(target.x, target.y, target.z);

	// 움직이는 광원
	/*static float rot = 0.0f;
	rot += 0.01f;
	auto light_pos = optix::Matrix4x4::rotate(rot, optix::make_float3(0, 1, 0)) *
		optix::Matrix4x4::translate(optix::make_float3(343.0f, 548.6f, 227.0f)) * optix::make_float4(0, 0, 0, 1);
	m_context["light_position"]->setFloat(optix::make_float3(light_pos.x, light_pos.y, light_pos.z));
	m_accumFrame = 0; */

	//m_context["gaze"]->setFloat(float(g_screenSize.cx * 0.5f), float(g_screenSize.cy - g_screenSize.cy * 0.5f));
}


void PathTracer::map_gl_buffer(const char *bufferName, GLuint tex)
{
	//auto texture = m_context["position_buffer"]->getBuffer()->get();
	
	optix::Buffer buffer = m_context[bufferName] ?
		m_context[bufferName]->getBuffer() : nullptr;
	
	if (!buffer) {
		printf_s("%s 라는 버퍼는 없습니다.\n", bufferName, strlen(bufferName));
		return;
	}
	printf_s("error code - %d\n", tex);

	auto buf = buffer->get();
	RTresult k = rtBufferCreateFromGLBO(m_context->get(), RT_BUFFER_INPUT, tex, &buf);
	//m_context["sampling_buffer"]->setBuffer((optix::Buffer&)*buf);

	//void const* data = buffer->map();
	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D, tex);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (GLsizei)g_screenSize.cx, (GLsizei)g_screenSize.cy, 0, GL_RGBA, GL_FLOAT, data); // BGRA8
	//buffer->unmap();

	//optix::TextureSampler sampling_map = m_context[bufferName]->getTextureSampler();
	//optix::TextureSampler new_sampler = m_context->createTextureSampler();
	//RTresult k = rtTextureSamplerCreateFromGLImage(m_context->get(), tex, RT_TARGET_GL_TEXTURE_2D, (RTtexturesampler*)&new_sampler);
	//m_context[bufferName]->setTextureSampler(new_sampler);

	printf_s("error code - %d (%d)\n", k, tex);
}


// 함수 구현
/**********************************************************************/
optix::TextureSampler createTextureSamplerFromBuffer(optix::Context & context, optix::Buffer buffer)
{
	optix::TextureSampler sampler = context->createTextureSampler();
	sampler->setWrapMode(0, RT_WRAP_REPEAT);
	sampler->setWrapMode(1, RT_WRAP_REPEAT);
	sampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
	sampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
	sampler->setMaxAnisotropy(4.f);
	sampler->setArraySize(1);
	sampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
	sampler->setMipLevelCount(1);
	sampler->setBuffer(0, 0, buffer);
	return sampler;
}

std::string load_file(const char * filepath)
{
	std::ifstream in(filepath);

	if (!in) {
		printf("%s 파일이 없습니다.", filepath);
		return std::string("");
	}

	std::stringstream stream;
	stream << in.rdbuf();

	return stream.str();
}