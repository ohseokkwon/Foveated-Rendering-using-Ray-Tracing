#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera
{
public:
	enum ProjMode {
		PM_Perspective,
		PM_Ortho_Height,
		PM_Ortho_Width,
		PM_Ortho,
	};

private:
	glm::vec2 m_screen;
	glm::vec4 m_viewport;
	glm::vec2 m_near_far;
	float m_aspect;
	ProjMode m_mode; // projection mode(3: perspective, ortho-height, ortho-width)
	float m_value; // perspective-fov or ortho-length
	glm::vec3 m_pos;
	glm::vec3 m_target;
	glm::quat m_rot;

	glm::vec3 m_prev_pos;
	glm::vec3 m_prev_up;
	glm::quat m_prev_rot;

	glm::mat4 m_prevMVP;
public:
	Camera();
	void setScreen(const glm::vec2& screen);
	void setViewport(const glm::vec4& viewport);
	void setProjectMode(ProjMode mode, float length_or_fov, float n, float f);
	void setPosition(const glm::vec3& pos);
	void setTarget(const glm::vec3& target);
	void setRotation(const glm::quat& rot);
	void translate(const glm::vec3& delta);
	void rotate(const float radiusAngle, const glm::vec3& axis);
	void rotateAround(const glm::vec3& target, const float radiusAngle, const glm::vec3& axis);
	void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));
	
	glm::vec3 getPosition() const;
	glm::vec3 getTarget() const;
	glm::quat getRotation() const;
	glm::vec3 getFront() const;
	glm::vec3 getBack() const;
	glm::vec3 getLeft() const;
	glm::vec3 getRight() const;
	glm::vec3 getUp() const;
	glm::vec3 getDown() const;
	
	glm::mat4 getVMat() const;
	const float *vmat() const;

	glm::mat4 getPMat() const;
	const float *pmat() const;

	bool isCursorInViewport(glm::vec2& pos) const;
	glm::vec3 getScreenToWorld(const glm::vec3& pos) const;
	glm::vec3 getWorldToScreen(const glm::vec3& pos) const;

	void applyViewport() const;
	void applyVPMat() const;

	void setPrevState();
	glm::vec3 getPrevPos() const;
	glm::vec3 getPrevUp() const;
	glm::mat4 getPrevMVP() const;
	glm::quat getPrevRotation() const;
};