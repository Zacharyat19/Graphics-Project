#ifndef _VERTEX_H_
#define _VERTEX_H_

#include <glm/glm.hpp>
#include <vector>

class Edge;
class Triangle;

// Represents a vertex in a mesh
class Vertex
{
public:
	size_t index;  // Index of this vertex in Mesh::vertices

	glm::vec3 position;  // Object-space position
	glm::vec3 normal;  // Object-space normal
	glm::vec2 texcoord;  // Texture coordinates

	std::vector<Edge *> edges;  // Edges adjacent to this vertex
	std::vector<Triangle *> triangles;  // Triangles adjacent to this vertex

	float distance;  // Distance from the vertex given to Mesh::compute_distances

	// ARAP Fields

	bool is_free;  // Can this vertex be optimized?
	glm::dvec3 pprime;  // Deformed position
	glm::dvec3 p_old;  // Undeformed position
	glm::dmat3 R;  // Rotation matrix for ARAP

	void compute_normal();

	Vertex();
	~Vertex();
private:
};

#endif
