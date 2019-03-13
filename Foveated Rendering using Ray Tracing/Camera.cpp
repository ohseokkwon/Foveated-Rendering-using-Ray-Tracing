#include <gl/glew.h>
#include "Camera.h"

Camera::Camera()
{
	m_mode = PM_Perspective;
	m_value = 45.f;
	m_screen = glm::vec2(1, 1);
	m_viewport = glm::vec4(0, 0, 1, 1);
	m_aspect = 1.f;
	m_pos = glm::vec3(0, 0, 0);
	m_rot = glm::quat(0, 0, 0, 1);
	m_near_far = glm::vec2(0.01f, 100.f);

	m_prevMVP = glm::mat4();
}

void Camera::setScreen(const glm::vec2 & screen)
{
	m_screen = screen;
}

void Camera::setViewport(const glm::vec4 & viewport)
{
	m_viewport = viewport;
	m_aspect = viewport[2] / viewport[3];
}

void Camera::setPosition(const glm::vec3 & pos)
{
	m_pos = pos;
}

void Camera::setTarget(const glm::vec3& target)
{
	m_target = target;
}
void Camera::setRotation(const glm::quat & rot)
{
	m_rot = rot;
}

void Camera::translate(const glm::vec3 & delta)
{
	m_pos += delta;
}

void Camera::rotate(const float radiusAngle, const glm::vec3 & axis)
{
	glm::quat rot = glm::angleAxis(radiusAngle, axis);

	// 회전
	m_rot = rot * m_rot;

	m_target -= m_pos;
	m_target = rot * m_target;
	m_target += m_pos;
}

void Camera::rotateAround(const glm::vec3 & target, const float radiusAngle, const glm::vec3 & axis)
{
	glm::quat rot = glm::angleAxis(radiusAngle, axis);

	// 회전
	m_rot = rot * m_rot;

	// 위치
	m_pos -= target;
	m_pos = rot * m_pos;
	m_pos += target;
}

void Camera::lookAt(const glm::vec3 & target, const glm::vec3 & up)
{
	glm::vec3 z = glm::normalize(m_pos - target); //+z is back(0, 0, 1) for me.
	glm::vec3 x = glm::normalize(glm::cross(up, z)); //+x
	glm::vec3 y = glm::normalize(glm::cross(z, x)); //+y
	glm::mat3 mat(x, y, z);

	m_rot = glm::normalize(glm::quat_cast(mat));

	m_target = target;
}

void Camera::setProjectMode(ProjMode mode, float length_or_fov, float n, float f)
{
	m_mode = mode;
	m_value = length_or_fov;

	m_near_far.x = n;
	m_near_far.y = f;
}

glm::vec3 Camera::getPosition() const
{
	return m_pos;
}

glm::vec3 Camera::getTarget() const
{
	return m_target;
}

glm::quat Camera::getRotation() const
{
	return m_rot;
}

glm::vec3 Camera::getFront() const
{
	return m_rot * glm::vec3(0, 0, -1);
}

glm::vec3 Camera::getBack() const
{
	return m_rot * glm::vec3(0, 0, 1);
}

glm::vec3 Camera::getLeft() const
{
	return m_rot * glm::vec3(-1, 0, 0);
}

glm::vec3 Camera::getRight() const
{
	return m_rot * glm::vec3(1, 0, 0);
}

glm::vec3 Camera::getUp() const
{
	return m_rot * glm::vec3(0, 1, 0);
}

glm::vec3 Camera::getDown() const
{
	return m_rot * glm::vec3(0, -1, 0);
}

glm::mat4 Camera::getVMat() const
{
	return glm::lookAt(m_pos, m_pos + getFront(), getUp());
}

const float * Camera::vmat() const
{
	static glm::mat4 mat;
	
	mat = getVMat();
	
	return &mat[0][0];
}

glm::mat4 Camera::getPMat() const
{
	if (m_mode == PM_Ortho_Height)
	{
		float y = m_value * 0.5f;
		float x = y * m_aspect;

		return glm::ortho(-x, x, -y, y, m_near_far[0], m_near_far[1]);
	}
	else if (m_mode == PM_Ortho_Width)
	{
		float x = m_value * 0.5f;
		float y = x / m_aspect;

		return glm::ortho(-x, x, -y, y, m_near_far[0], m_near_far[1]);
	}
	else if (m_mode == PM_Ortho)
	{
		float x = (m_aspect > 1.f) ? (m_value * 0.5f) * m_aspect : m_value * 0.5f;
		float y = (m_aspect > 1.f) ? m_value * 0.5f : (m_value * 0.5f) / m_aspect;

		return glm::ortho(-x, x, -y, y, m_near_far[0], m_near_far[1]);
		//return glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, m_near_far[0], m_near_far[1]);
	}
	else
	{
		return glm::perspective(glm::radians(m_value), m_aspect, m_near_far[0], m_near_far[1]);
	}
}

const float * Camera::pmat() const
{
	static glm::mat4 mat;

	mat = getPMat();

	return &mat[0][0];
}

bool Camera::isCursorInViewport(glm::vec2 & pos) const
{
	// flip y
	glm::vec2 p(pos.x, m_screen.y - pos.y);

	return
		p.x >= m_viewport[0] &&
		p.x <= m_viewport[0] + m_viewport[2] &&
		p.y >= m_viewport[1] &&
		p.y <= m_viewport[1] + m_viewport[3];
}

glm::vec3 Camera::getScreenToWorld(const glm::vec3 & pos) const
{
	glm::vec3 p(pos.x, m_viewport[3] - pos.y, pos.z);
	return glm::unProject(p, getVMat(), getPMat(), m_viewport);
}

glm::vec3 Camera::getWorldToScreen(const glm::vec3 & pos) const
{
	return glm::project(pos, getVMat(), getPMat(), m_viewport);
}

void Camera::applyViewport() const
{
	glViewport((GLsizei)m_viewport[0], (GLsizei)m_viewport[1], 
		(GLsizei)m_viewport[2], (GLsizei)m_viewport[3]);
}

void Camera::applyVPMat() const
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(pmat());
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(vmat());
}

glm::vec3 Camera::getPrevPos() const
{
	return m_prev_pos;
}

void Camera::setPrevState()
{
	m_prev_pos = m_pos;
	m_prev_rot = m_rot;
	m_prevMVP = getPMat() * getVMat();
	m_prev_up = getUp();

}

glm::vec3 Camera::getPrevUp() const
{
	return m_prev_up;
}

glm::mat4 Camera::getPrevMVP() const
{
	return m_prevMVP;
}

glm::quat Camera::getPrevRotation() const
{
	return m_prev_rot;
}