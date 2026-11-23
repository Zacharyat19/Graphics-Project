#include "edge.h"

#include "vertex.h"

void Edge::compute_weight(Weights weight_type)
{
	switch (weight_type)
	{
	default:
	case UniformWeights:
		weight = 1.0;
		break;
	case CordWeights:
		// TODO (550 only): Compute cord weighting for this edge.
		weight = 1.0 / glm::length(vertices[0]->p_old - vertices[1]->p_old);
		break;
	}
}

Edge::Edge() :
	index(-1),
	weight(1.0f),
	vertices{ nullptr, nullptr },
	triangles{ nullptr, nullptr }
{

}

Edge::~Edge()
{

}
