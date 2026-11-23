#include "vertex.h"

#include "triangle.h"

void Vertex::compute_normal()
{
	glm::vec3 n(0.0f);
	for (Triangle *t : triangles)
		n += t->area * t->normal;
	normal = glm::normalize(n);
}

Vertex::Vertex() :
	index(-1),
	position(0.0f),
	normal(0.0f),
	texcoord(0.0f, 0.0f),
	distance(0.0f),
	is_free(false),
	pprime(0.0),
	p_old(0.0),
	R(1.0)
{

}

Vertex::~Vertex()
{

}
