#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <glm/glm.hpp>
#include <glm/ext/quaternion_float.hpp>

class Mesh;
class Vertex;

struct Ray
{
    glm::vec3 p;
	glm::vec3 d;

	inline glm::vec3 operator()(float t) const { return p + t*d; }
	inline float isect_plane(const glm::vec3 &p0, const glm::vec3 &n) const { return glm::dot(n,p0-p)/glm::dot(n,d); }  
};

// Represents a camera orbiting around a position in 3D space.
class Camera
{
public:
	enum Type
	{
		ORTHOGRAPHIC,  // Orthographic projection
		PERSPECTIVE    // Perspective projection
	};

	// Position to orbit around
	const glm::vec3 &target() const;
	void set_target(const glm::vec3 &target);

	// Distance from orbit target
	float target_distance() const;
	void set_target_distance(float target_distance);

	// Rotation around orbit point
	const glm::quat &orientation() const;
	void set_orientation(const glm::quat &q);

	Type type() const;
	void set_type(Type type);

	// Viewport dimensions
	void set_size(int width, int height);
	int width() const;
	int height() const;

	// Orthographic scale
	float orthographic_scale() const;
	void set_orthographic_scale(float scale);

	void validate();

	// Projection matrix
	const glm::mat4 &projection();

	// View matrix
	const glm::mat4 &view();

	// Eye position
	const glm::vec3 &position();

	// Front vector
	const glm::vec3 &front();

	// Right vector
	const glm::vec3 &right();

	// Up vector
	const glm::vec3 &up();

	void trackball(const glm::vec2 &old_pos, const glm::vec2 &new_pos);

	// Reset camera to default values
	void reset();

	// Convert NDC mouse coords to world-space ray
	Ray ndc_to_world_ray(const glm::vec2 &ndc);

	Vertex *pick_vertex(const Mesh *mesh, const glm::vec2 &mouse_pos_ndc);

	Camera();
	~Camera() = default;
private:
	enum ValidFlags
	{
		DIRTY_PROJECTION = 0x1,
		DIRTY_VIEW = 0x2
	};

	glm::vec3 _target;
	float _target_distance;
	glm::quat _orientation;

	Type _type;
	int _width, _height;
	float _orthographic_scale;

	unsigned _dirty_mask;

	glm::mat4 _projection;
	glm::mat4 _view;

	glm::vec3 _position;
	glm::vec3 _front, _right, _up;
};

#endif
