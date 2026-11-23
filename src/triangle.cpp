#include "triangle.h"

#include "vertex.h"

void Triangle::compute_normal_and_area()
{
	glm::vec3 u = vertices[1]->position - vertices[0]->position;
	glm::vec3 v = vertices[2]->position - vertices[0]->position;

	glm::vec3 uxv = glm::cross(u, v);

	area = glm::length(uxv) / 2.0f;
	normal = glm::normalize(uxv);
}

std::array<glm::vec3, 3> Triangle::transform_vertices(const glm::mat4 &T) const
{
	glm::vec4 v0 = T * glm::vec4(vertices[0]->position, 1.0f);
	glm::vec4 v1 = T * glm::vec4(vertices[1]->position, 1.0f);
	glm::vec4 v2 = T * glm::vec4(vertices[2]->position, 1.0f);
	return { glm::vec3(v0) / v0.w, glm::vec3(v1) / v1.w, glm::vec3(v2) / v2.w };
}

Triangle::Triangle() :
	index(-1),
	normal(0.0f),
	area(0.0f),
	vertices{nullptr, nullptr, nullptr},
	edges{nullptr, nullptr, nullptr}
{

}

Triangle::~Triangle()
{

}
