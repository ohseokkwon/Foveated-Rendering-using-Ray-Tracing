#pragma once

#define NOMINMAX
#include <optix.h>
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <optixu/optixu_aabb_namespace.h>
#include <map>

class PathTracer
{
public:
	enum TextureName {
		POSITION,
		NORMAL,
		DEPTH,
		DIFFUSE,
		WEIGHT,
		THREAD,
		HISTORY,
		SHADING,
		EXTRA,

		/*Color = 0,
		Normal = 1,
		Position = 2,
		Depth = 3,
		Diffuse,
		History,
		Weight,*/
	};

private:
	/************************************************************************/
	// Optix
	/************************************************************************/
	enum MaterialType {
		MATL_DIFFUSE,
		MATL_REFLECTION,
		MATL_REFRACTION,
	};
	struct Model {
		std::string m_mesh_filename;
		std::string m_texture_name;
		optix::Matrix4x4 m_mesh_xforms;
		MaterialType m_matl_type;

		optix::Material m_material;
	};

	int m_width;
	int m_height;
	bool m_is_valid = false;

	// Context, Buffer

	// Program, Variable
	std::map<std::string, optix::Program> m_mapOfPrograms;
	optix::float3 m_colorBackground;
	optix::Buffer m_light_buffer;

	// Geometry, Material, AABB
	std::vector<Model> m_model_info;

	optix::Group m_top_group;
	optix::Aabb m_aabb;

	/************************************************************************/
	// GL
	/************************************************************************/
	GLuint m_textures[EXTRA+1] = { 0, }; // 0: color, 1: normal, 2: position, 3: depth, 4: weight, 5: history

public:
	PathTracer() = default;
	virtual ~PathTracer();

	bool initialize(int width, int height);
	void init_camera(const Camera& camera);
	void update_optix_variables(const Camera& camera);

	float geometry_launch();
	float sampling_launch();
	float optimize_launch();
	float shading_launch();

	void update_textures();
	void render(int texNum = 0);

	GLuint& get_texture(TextureName name);
	const GLuint& get_texture(TextureName name) const;
	unsigned int m_accumFrame = 0;
	optix::Context m_context;

	void map_gl_buffer(const char *bufferName, GLuint tex);

private:
	void init_context();
	void init_program();
	void init_buffers();
	void init_geometry();

	optix::Material load_obj(Model& model);

	optix::Aabb PathTracer::createGeometry(std::vector<Model>& models);

	bool update_gl_texture(const char* buffer_name, GLuint& opengl_texture_id);

	void swapBuffer(const char* a_buffer, const char* b_buffer);
};