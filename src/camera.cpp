#include "camera.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

const glm::vec3 &Camera::target() const
{
	return _target;
}

void Camera::set_target(const glm::vec3 &target)
{
	_target = target;
	_dirty_mask |= DIRTY_VIEW;
}

float Camera::target_distance() const
{
	return _target_distance;
}

void Camera::set_target_distance(float target_distance)
{
	_target_distance = target_distance;
	_dirty_mask |= DIRTY_VIEW;
}

const glm::quat &Camera::orientation() const
{
	return _orientation;
}

void Camera::set_orientation(const glm::quat &q)
{
	_orientation = q;
	_dirty_mask |= DIRTY_VIEW;
}

Camera::Type Camera::type() const
{
	return _type;
}

void Camera::set_type(Type type)
{
	_type = type;
	_dirty_mask |= DIRTY_PROJECTION;
}

void Camera::set_size(int width, int height)
{
	_width = width;
	_height = height;
	_dirty_mask |= DIRTY_PROJECTION;
}

int Camera::width() const
{
	return _width;
}

int Camera::height() const
{
	return _height;
}

float Camera::orthographic_scale() const
{
	return _orthographic_scale;
}

void Camera::set_orthographic_scale(float scale)
{
	_orthographic_scale = scale;
	_dirty_mask |= DIRTY_PROJECTION;
}

void Camera::validate()
{
	if (_dirty_mask & DIRTY_PROJECTION)
	{
		// Compute projection matrix

		float aspect = static_cast<float>(_width) / static_cast<float>(_height);
		if (_type == PERSPECTIVE)
		{
			float fov = glm::radians(54.0f);
			if (aspect < 1.0f)
				fov = 2.0f * atanf(tanf(fov * 0.5f) / aspect);
			_projection = glm::perspective(fov, aspect, 0.1f, 100.0f);
		}
		else
		{
			if (aspect > 1.0f)
				_projection = glm::ortho(-aspect*_orthographic_scale, aspect*_orthographic_scale, -_orthographic_scale, _orthographic_scale, 0.0f, 100.0f);
			else
			{
				float inv_aspect = 1.0f / aspect;
				_projection = glm::ortho(-_orthographic_scale, _orthographic_scale, -inv_aspect*_orthographic_scale, inv_aspect*_orthographic_scale, 0.0f, 100.0f);
			}
		}
	}

	if (_dirty_mask & DIRTY_VIEW)
	{
		// Compute view matrix and vectors

		_front = glm::normalize(_orientation * glm::vec3(0.0f, 0.0f, -1.0f));
		_right = glm::normalize(_orientation * glm::vec3(1.0f, 0.0f, 0.0f));
		_up = glm::normalize(_orientation * glm::vec3(0.0f, 1.0f, 0.0f));

		_position = _target - _target_distance * _front;
		_view = glm::lookAt(_position, _target, _up);
	}

	_dirty_mask = 0;
}

const glm::mat4 &Camera::projection()
{
	validate();
	return _projection;
}

const glm::mat4 &Camera::view()
{
	validate();
	return _view;
}

const glm::vec3 &Camera::position()
{
	validate();
	return _position;
}

const glm::vec3 &Camera::front()
{
	validate();
	return _front;
}

const glm::vec3 &Camera::right()
{
	validate();
	return _right;
}

const glm::vec3 &Camera::up()
{
	validate();
	return _up;
}

static glm::vec3 project_to_sphere(
	const glm::vec2 &pos,  // mouse position in pixels relative to top-left of screen
	const glm::vec2 &dim   // size of the viewport in pixels
)
{
	float nx = (2.0f*pos.x - dim.x) / dim.x;
	float ny = (dim.y - 2.0f*pos.y) / dim.y;
	float len2 = nx*nx + ny*ny;
	if (len2 < 1.0f)
		return glm::vec3(nx, ny, sqrtf(1.0f - len2));
	float len = sqrtf(len2);
	return glm::vec3(nx / len, ny / len, 0.0f);
}

void Camera::trackball(const glm::vec2 &old_pos, const glm::vec2 &new_pos)
{
	glm::vec2 dim(_width, _height);
	glm::vec3 p0 = project_to_sphere(old_pos, dim);  // old position on sphere
	glm::vec3 p1 = project_to_sphere(new_pos, dim);  // new position on sphere

	float dot = glm::clamp(glm::dot(p0, p1), -1.0f, 1.0f);
	float angle = acosf(dot);
	glm::vec3 axis = glm::cross(p0, p1);
	if (glm::length2(axis) > 1e-8f)
	{
		axis = glm::normalize(axis);
		glm::quat q = glm::angleAxis(angle, axis);
		q = glm::inverse(q);
		_orientation = glm::normalize(_orientation * q);
	}

	_dirty_mask |= DIRTY_VIEW;  // marks the view as dirty, keep this
}

void Camera::reset()
{
	_target = glm::vec3(0.0f);
	_target_distance = 2.0f;
	_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	_type = PERSPECTIVE;
	_orthographic_scale = 1.0f;

	_dirty_mask = DIRTY_VIEW | DIRTY_PROJECTION;
}

Camera::Camera() :
	_target(0.0f),
	_target_distance(2.0f),
	_orientation(1.0f, 0.0f, 0.0f, 0.0f),
	_type(PERSPECTIVE),
	_width(1), _height(1),
	_orthographic_scale(1.0f),
	_dirty_mask(-1),
	_projection(0.0f),
	_view(0.0f),
	_position(0.0f),
	_front(0.0f), _right(0.0f), _up(0.0f)
{
}
