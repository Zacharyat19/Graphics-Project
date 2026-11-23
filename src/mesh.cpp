#include "mesh.h"

#include "vertex.h"
#include "edge.h"
#include "triangle.h"

#include "ply.h"

#include <glm/gtc/matrix_transform.hpp>

#include <set>

typedef struct {
	float x, y, z;
	void *other_properties;
} PlyVertex;

typedef struct {
	unsigned char nverts;
	int *vertices;
	void *other_properties;
} PlyFace;

static bool read_ply_file(const char *file_name, std::vector<Vertex *> *vertices, std::vector<Triangle *> *triangles)
{
	int element_count;
	char *element_name;
	FILE *in_file;

	// Read in the original PLY object
	in_file = fopen(file_name, "r");
	if (in_file == NULL)
	{
		printf("[read_ply_file] Cannot open file `%s'", file_name);
		return false;
	}

	PlyFile *in_ply = read_ply(in_file);
	if (!in_ply)
	{
		fclose(in_file);
		printf("[read_ply_file] Failed to read file `%s'", file_name);
		return false;
	}

	PlyProperty vertex_properties[] = { // list of property information for a vertex 
		{const_cast<char *>("x"), Float32, Float32, offsetof(PlyVertex,x), 0, 0, 0, 0},
		{const_cast<char *>("y"), Float32, Float32, offsetof(PlyVertex,y), 0, 0, 0, 0},
		{const_cast<char *>("z"), Float32, Float32, offsetof(PlyVertex,z), 0, 0, 0, 0},
	};

	PlyProperty face_properties[] = { // list of property information for a face 
		{const_cast<char *>("vertex_indices"), Int32, Int32, offsetof(PlyFace,vertices), 1, Uint8, Uint8, offsetof(PlyFace,nverts)}
	};

	for (int i = 0; i < in_ply->num_elem_types; i++)
	{
		// prepare to read the i'th list of elements 
		element_name = setup_element_read_ply(in_ply, i, &element_count);

		if (equal_strings("vertex", element_name)) {

			// set up for getting vertex elements 
			setup_property_ply(in_ply, &vertex_properties[0]);
			setup_property_ply(in_ply, &vertex_properties[1]);
			setup_property_ply(in_ply, &vertex_properties[2]);
			get_other_properties_ply(in_ply, offsetof(PlyVertex, other_properties));

			// grab all the vertex elements 
			PlyVertex *ply_vert = new PlyVertex;
			for (int j = 0; j < element_count; j++)
			{
				get_element_ply(in_ply, ply_vert);
				Vertex *new_vert = new Vertex;
				new_vert->position = glm::vec3(ply_vert->x, ply_vert->y, ply_vert->z);
				new_vert->index = vertices->size();
				vertices->push_back(new_vert);
			}
			delete ply_vert;
		}

		else if (equal_strings("face", element_name)) {

			// set up for getting face elements
			setup_property_ply(in_ply, &face_properties[0]);
			get_other_properties_ply(in_ply, offsetof(PlyFace, other_properties));

			// grab all the face elements 
			PlyFace *ply_face = new PlyFace;
			for (int j = 0; j < element_count; j++)
			{
				get_element_ply(in_ply, ply_face);
				Triangle *t = new Triangle;
				t->vertices[0] = reinterpret_cast<Vertex *>(ply_face->vertices[0]);
				t->vertices[1] = reinterpret_cast<Vertex *>(ply_face->vertices[1]);
				t->vertices[2] = reinterpret_cast<Vertex *>(ply_face->vertices[2]);

				t->index = triangles->size();
				triangles->push_back(t);
			}
			delete ply_face;
		}
		else
			get_other_element_ply(in_ply);
	}

	free_ply(in_ply);
	fclose(in_file);

	// fix indices
	for (size_t i = 0; i < triangles->size(); ++i)
	{
		Triangle *t = (*triangles)[i];
		for (size_t j = 0; j < 3; ++j)
		{
			size_t index = reinterpret_cast<size_t>(t->vertices[j]);
			if (index >= vertices->size())
			{
				printf("[read_ply_file] triangle vertex index out of bounds\n");
				goto failure;
			}
			Vertex *v = t->vertices[j] = (*vertices)[index];
			v->triangles.push_back(t);
		}
	}

	return true;

failure:
	for (Vertex *v : *vertices)
		delete v;
	vertices->clear();

	for (Triangle *t : *triangles)
		delete t;
	triangles->clear();

	return false;
}

static Edge *find_edge(const Vertex *v0, const Vertex *v1)
{
	for (Edge *e : v0->edges)
	{
		if (e->vertices[0] == v1 || e->vertices[1] == v1)
			return e;
	}

	return nullptr;
}

static bool build_edges(const std::vector<Triangle *> &triangles, std::vector<Edge *> *edges)
{
	for (Triangle *t : triangles)
	{
		for (size_t i = 0; i < 3; ++i)
		{
			Vertex *v0 = t->vertices[i];
			Vertex *v1 = t->vertices[(i + 1) % 3];

			Edge *e = find_edge(v0, v1);
			if (e == nullptr)
			{
				e = new Edge;
				e->vertices[0] = v0;
				e->vertices[1] = v1;
				e->index = edges->size();
				edges->push_back(e);

				v0->edges.push_back(e);
				v1->edges.push_back(e);
			}

			if (e->triangles[0] == nullptr)
				e->triangles[0] = t;
			else if (e->triangles[1] == nullptr)
				e->triangles[1] = t;
			else
			{
				printf("[build_edges] non-manifold input\n");
				goto failure;
			}

			t->edges[i] = e;
		}
	}

	return true;

failure:
	for (Edge *e : (*edges))
		delete e;
	edges->clear();
	return false;
}

void Mesh::compute_normals_and_areas()
{
	for (Triangle *t : triangles)
		t->compute_normal_and_area();
	for (Vertex *v : vertices)
		v->compute_normal();
}

void Mesh::compute_properties()
{
	compute_normals_and_areas();

	float sum_areas = 0.0f;
	centroid = glm::vec3(0.0f);
	for (Triangle *t : triangles)
	{
		glm::vec3 center = (t->vertices[0]->position + t->vertices[1]->position + t->vertices[2]->position) / 3.0f;
		centroid += t->area * center;
		sum_areas += t->area;
	}
	centroid /= sum_areas;

	radius = -HUGE_VALF;
	for (Vertex *v : vertices)
		radius = glm::max(radius, glm::length(v->position - centroid));

	model = glm::mat4(1.0f);
	model = glm::scale(model, glm::vec3(1.0f / radius));
	model = glm::translate(model, -centroid);
}

void Mesh::compute_distances(Vertex *v_src)
{
	// Compare two vertices, sorting by smallest distance first
	struct CompareVertex
	{
		constexpr bool operator()(const Vertex *a, const Vertex *b) const noexcept
		{
			return a->distance < b->distance;
		}
	};

	std::set<Vertex *, CompareVertex> Q;

	// All vertices are initially infinitely far away from the source vertex
	for (Vertex *v : vertices)
		v->distance = INFINITY;

	// The source vertex is 0 distance from itself, insert it into the queue
	v_src->distance = 0.0f;
	Q.insert(v_src);

	// TODO: Implement Dijkstra's algorithm to compute the distance from the
	// vertex v_src for every vertex.
	// Hint: The set Q is sorted such that the front of the set is the vertex
	// that is the smallest distance away from the source. Store the distance
	// in the member Vertex::distance. Use the Euclidean norm (glm::length) to
	// compute the distance between two adjacent vertices when computing total
	// distance. The following member functions of std::set will be helpful:
	// - begin(): Returns an iterator to the front of the set. Dereference
	//            the return value to retrieve the Vertex.
	// - erase(v): Remove a value from the set, if it exists.
	// - insert(v): Insert a value into the set, if it wasn't already in the
	//              set.
	// - empty(): Returns true if the set is empty (i.e. size()==0)
	// Note: Updating the distance of a vertex *will not* update its position
	// in Q! Remove the vertex whose distance you want to change from the set,
	// then update its distance and reinsert it.
	while(!Q.empty())
	{
		Vertex *v = *Q.begin();
		Q.erase(v);

		for(Edge *e : v->edges)
		{
			Vertex *v2;
			if(e->vertices[0] == v)
			{
				v2 = e->vertices[1];
			}
			else
			{
				v2= e->vertices[0];
			}

			float dist = v->distance + glm::length(v2->position - v->position);
			if(dist < v2->distance)
			{
				Q.erase(v2);
				v2->distance = dist;
				Q.insert(v2);
			}
		}
	}
}

Mesh::Mesh(const char *file) :
	radius(1.0f),
	centroid(0.0f)
{
	if (!read_ply_file(file, &vertices, &triangles))
		exit(1);

	if (!build_edges(triangles, &edges))
		exit(1);

	compute_properties();
}

Mesh::~Mesh()
{
	for (Vertex *v : vertices)
		delete v;

	for (Edge *e : edges)
		delete e;

	for (Triangle *t : triangles)
		delete t;
}
