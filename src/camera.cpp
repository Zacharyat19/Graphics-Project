#include "camera.h"

#include <glm/ext/quaternion_trigonometric.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "mesh.h"
#include "triangle.h"
#include "vertex.h"

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

	_type = ORTHOGRAPHIC;
	_orthographic_scale = 1.0f;

	_dirty_mask = DIRTY_VIEW | DIRTY_PROJECTION;
}

Ray Camera::ndc_to_world_ray(const glm::vec2 &ndc)
{
	validate();

	glm::vec4 near_clip(ndc[0], ndc[1], -1.0f, 1.0f);
	glm::vec4 far_clip(ndc[0], ndc[1], 1.0f, 1.0f);
	
	glm::mat4 invMPV = glm::inverse(_projection * _view);
	
	glm::vec4 near_eye = invMPV * near_clip;
	glm::vec4 far_eye = invMPV * far_clip;
	
	glm::vec3 A = glm::vec3(near_eye) / near_eye.w;
	glm::vec3 B = glm::vec3(far_eye)/ far_eye.w;
	
	return Ray{ A, glm::normalize(B - A) };
}

static bool ray_triangle_isect(Triangle *tri, const glm::vec3 &origin, const glm::vec3 &dir, float *isect_t)
{
	const float eps = 1e-11f;

	const glm::vec3 verts[3] = {
		tri->vertices[0]->position,
		tri->vertices[1]->position,
		tri->vertices[2]->position,
	};
	
	const glm::vec3 &v1 = verts[0], &v2 = verts[1], &v3 = verts[2];
	*isect_t = NAN;
	glm::vec3 e1 = v2 - v1;
	glm::vec3 e2 = v3 - v1;
	glm::vec3 h = glm::cross(dir, e2); //ray.d.cross(e2);
	float a = glm::dot(e1, h); //e1.dot(h);
	if (a > -eps && a < eps)
		return false; // parallel to triangle
	float f = 1.0 / a;
	glm::vec3 s = origin - v1;
	float u = f * glm::dot(s, h); //s.dot(h);
	if (u < 0.0 || u > 1.0)
		return false; // outside triangle  
	glm::vec3 q = glm::cross(s, e1); //s.cross(e1);
	float v = f * glm::dot(dir, q); //ray.d.dot(q);
	if (v < 0.0 || u + v > 1.0)
		return false; // outside triangle 
	float t_out = f * glm::dot(e2, q);// e2.dot(q);
	if (t_out <= eps)
		return false; // behind origin   
	*isect_t = t_out;
	return true;


	/*glm::vec3 e1 = tri->vertices[1]->position - tri->vertices[0]->position;
	glm::vec3 e2 = tri->vertices[2]->position - tri->vertices[0]->position;

	glm::vec3 h = glm::cross(dir, e2);
	float a = glm::dot(e1, h);
	if (a > -eps || a < eps)
		return false;
	float f = 1.0f / a;
	glm::vec3 s = origin - tri->vertices[1]->position;
	float u = f * glm::dot(s, h);
	if (u < 0.0f || u > 1.0f)
		return false;
	glm::vec3 q = glm::cross(s, e1);
	float v = f * glm::dot(dir, q);
	if (v < 0.0f || u + v > 1.0f)
		return false;
	float t_out = f * glm::dot(e2, q);
	if (t_out <= eps)
		return false;
	*isect_t = t_out;
	return true;*/




	/*glm::vec3 normal = glm::cross(e1, e2);

	if (glm::dot(normal, dir) > 0.0f)
		return false;  // Triangle faces away from camera

	glm::vec3 dir_cross_e2 = glm::cross(dir, e2);
	float det = glm::dot(e1, dir_cross_e2);
	if (glm::abs(det) < 1e-11f)
		return false;  // Ray parallel to triangle
	
	float inv_det = 1.0f / det;
	glm::vec3 s = origin - tri->vertices[0]->position;
	float u = inv_det * glm::dot(s, dir_cross_e2);

	if (u < -1e-11f || u-1.0f > 1e-11f)
		return false;

	glm::vec3 s_cross_e1 = glm::cross(s, e1);
	float v = inv_det * glm::dot(dir, s_cross_e1);

	if (v < -1e-11f || u + v - 1.0f > 1e-11f)
		return false;

	float t = inv_det * glm::dot(e2, s_cross_e1);
	if (t > 1e-11f)
	{
		*isect_t = t;
		return true;
	}

	return false;*/
}

Vertex *Camera::pick_vertex(const Mesh *mesh, const glm::vec2 &mouse_pos_ndc)
{
	// Convert ndc -> object space
	glm::mat4 inv_MVP = glm::inverse(_projection * _view * mesh->model);
	glm::vec4 n = inv_MVP * glm::vec4(mouse_pos_ndc, -1.0f, 1.0f);
	glm::vec4 f = inv_MVP * glm::vec4(mouse_pos_ndc, 1.0f, 1.0f);
	
	// Ray
	glm::vec3 origin = glm::vec3(n) / n.w;
	glm::vec3 dir = glm::normalize((glm::vec3(f) / f.w) - origin);

	Triangle *closest_tri = nullptr;
	float closest_t = HUGE_VALF;
	for (Triangle *tri : mesh->triangles)
	{
		float t;
		if (ray_triangle_isect(tri, origin, dir, &t))
		{
			if (t < closest_t)
			{
				closest_tri = tri;
				closest_t = t;
			}
		}
	}

	if (!closest_tri)
		return nullptr;

	glm::vec3 isect_pos = origin + closest_t*dir;

	Vertex *closest_vert;
	float closest_dist = HUGE_VALF;
	for (int i = 0; i < 3; ++i)
	{
		float d = glm::length(isect_pos - closest_tri->vertices[i]->position);
		if (d < closest_dist)
		{
			closest_vert = closest_tri->vertices[i];
			closest_dist = d;
		}
	}

	return closest_vert;
}

Camera::Camera() :
	_dirty_mask(-1),
	_target(0.0f),
	_target_distance(2.0f),
	_orientation(1.0f, 0.0f, 0.0f, 0.0f),
	_type(ORTHOGRAPHIC),
	_width(1), _height(1),
	_orthographic_scale(1.0f),
	_projection(0.0f),
	_view(0.0f),
	_position(0.0f),
	_front(0.0f), _right(0.0f), _up(0.0f)
{

}
