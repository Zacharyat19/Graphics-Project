#ifndef _EDGE_H_
#define _EDGE_H_

#include <glm/glm.hpp>
#include <array>

class Triangle;
class Vertex;

// Represents an edge in a mesh
class Edge
{
public:
	enum Weights
	{
		UniformWeights,
		CordWeights
	};

	size_t index;  // Index of this edge in Mesh::edges

	double weight;  // This edge's weight (w_ij)

	std::array<Vertex *, 2> vertices;  // This edge's vertices
	std::array<Triangle *, 2> triangles;  // This edge's triangles

	void compute_weight(Weights weight_type);

	Edge();
	~Edge();
private:
};

#endif
