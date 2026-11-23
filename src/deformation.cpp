#include "deformation.h"

#include "mesh.h"
#include "edge.h"
#include "vertex.h"

#include <cassert>
#include <utility>
#include <cstdio>
#include <cmath>

#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

bool Deformation::set_actuator(Vertex *vertex)
{
    if (_status != Stopped)
        return false;

    if (vertex != _act_vertex)
    {
        _act_vertex = vertex;
        _valid = false;

        if (_act_vertex)
        {
            printf("[ARAP] Set actuator vertex: %zu\n", vertex->index);

            _mesh->compute_distances(_act_vertex);
        }

        validate_regions();
    }

    return true;
}

bool Deformation::set_actuation_distance(float distance)
{
    if (_status != Stopped)
        return false;

    _act_distance = glm::max(distance, 0.0f);
    _valid = false;

    printf("Actuation Distance: %g\n", (double)_act_distance);
    return true;
}

bool Deformation::set_weight_type(Edge::Weights weights)
{
    if (_status != Stopped)
        return false;
    
    if (_edge_weights != weights)
    {
        switch (weights)
        {
        default:
            return false;
        case Edge::UniformWeights:
            printf("[ARAP] Using uniform weights\n");
            break;
        case Edge::CordWeights:
            printf("[ARAP] Using cord weights\n");
            break;
        }

        _edge_weights = weights;
    }

    return true;
}

void Deformation::validate_regions()
{
    if (_status != Stopped)
        return;
    if (_valid)
        return;

    _free_vertices.clear();
    _constrained_vertices.clear();
    _boundary_vertices.clear();

    if (_act_vertex)
    {
        // Free vertices are those within the actuation distance and not the actuation vertex
        for (Vertex *v : _mesh->vertices)
        {
            if (v != _act_vertex && v->distance <= _act_distance)
            {
                _free_vertices.push_back(v);
                v->is_free = true;
            }
            else
            {
                _constrained_vertices.push_back(v);
                v->is_free = false;
            }
        }

        if (_constrained_vertices.size() == 1)
        {
            printf("[ARAP] Warning: actuation distance too large, choose tighter constraints\n");

            _free_vertices.clear();
            _boundary_vertices.clear();
            _constrained_vertices.clear();
            return;
        }

        // Boundary vertices are vertices that are constrained but have a free 1-ring neighbor
        for (Vertex *v_i : _constrained_vertices)
        {
            for (Edge *e : v_i->edges)
            {
                Vertex *v_j = e->vertices[0] == v_i ? e->vertices[1] : e->vertices[0];
                if (v_j->is_free)
                {
                    _boundary_vertices.push_back(v_i);
                    break;
                }
            }
        }

        printf("[ARAP] Constraints: %zu\n", _constrained_vertices.size());
    }
    else
    {
        for (Vertex *v_i : _mesh->vertices)
            v_i->is_free = true;
    }

    _valid = true;
}

void Deformation::set_mesh_as_target()
{
    if (_status != Stopped)
        return;
    for (Vertex *v : _mesh->vertices)
        v->p_old = glm::dvec3(v->position);
}

// Run the solver. The function may return before convergence, check the return value
// of status(). If Paused, the solver has not converged, but instead has taken too much
// time. Call solve again to resume the solver. If Stopped, the deformation has finished
// and another call to solve() will start a new optimization. When deformation finishes,
// the actuator and constraints are reset and new ones will need to be specified.
void Deformation::solve()
{
    if (_status == Running)
    {
        printf("[ARAP] Warning: Recursive solve()\n");
        return;
    }

    // Check if this is a new deformation run
    if (_status == Stopped)
    {
        validate_regions();

        if (_free_vertices.empty())
        {
            printf("[ARAP] No free vertices\n");
            return;
        }

        printf("========================================\n");
        printf("[ARAP] Begin Deformation\n");
        printf("[ARAP] Free vertices: %zu\n", _free_vertices.size());
        printf("[ARAP] Constrained vertices: %zu\n", _constrained_vertices.size());
        printf("[ARAP] Max Iterations: %d\n", MaxIterations);
        printf("[ARAP] Max Inner Iterations: %d\n", MaxInnerIterations);
        printf("[ARAP] Min Absolute Error: %g\n", MinAbsError);
        printf("[ARAP] Min Delta Error: %g\n", MinDeltaError);
        printf("[ARAP] Min Relative Error: %g\n", MinRelativeError);
        printf("[ARAP] Max Iteration Time: %g\n", MaxIterationTime);

        // Initialize pprime with the current vertex positions and initial rotations
        for (Vertex *v : _mesh->vertices)
        {
            v->pprime = glm::dvec3(v->position);
            v->R = glm::dmat3(1.0);
        }

        // Initialize edge weights
        for (Edge *e : _mesh->edges)
            e->compute_weight(_edge_weights);

        _outer_iterations = 0;
        _last_energy = compute_energy();

        printf("[ARAP] Initial Energy: %f\n", (double)_last_energy);
    }

    _status = Running;
 
    // Epoch at which we should pause the deformation and return control
    // to the caller
    double timeout_epoch = glfwGetTime() + MaxIterationTime;

    int start_iteration = _outer_iterations;

    double energy;
    for (;;)
    {
        // Run one ARAP iteration
        compute_rotation_matrices();
        solve_vertex_positions();

        // Increment iteration count and compute the energy
        _outer_iterations++;
        energy = compute_energy();

        // Check absolute error convergence
        if (energy <= MinAbsError)
        {
            printf("[ARAP] Converged (absolute error)\n");
            _status = Stopped;
            break;
        }
        
        // Check absolute difference
        double delta_error = std::abs(_last_energy - energy);
        if (delta_error <= MinDeltaError)
        {
            printf("[ARAP] Converged (delta error)\n");
            _status = Stopped;
            break;
        }

        // Check relative error convergence
        double rel_error = delta_error / energy;
        if (rel_error <= MinRelativeError)
        {
            printf("[ARAP] Converged (relative error)\n");
            _status = Stopped;
            break;
        }

        // Check if maximum iterations exceeded
        if (_outer_iterations >= MaxIterations)
        {
            printf("[ARAP] Did not converge (maximum iterations)\n");
            _status = Stopped;
            break;
        }

        _last_energy = energy;

        // Check if the application time has passed the timeout epoch
        if (glfwGetTime() >= timeout_epoch)
        {
            // Too much time passed, pasue the solver and return to the caller
            store_deformed_positions();
            _mesh->compute_normals_and_areas();
            printf("[ARAP] %d:%d timeout, energy %g, delta %g, rel %g\n",  start_iteration, _outer_iterations, energy, delta_error, rel_error);
            _status = Paused;
            return;
        }
    }

    assert(_status == Stopped);

    // Update the mesh with the new positions
    store_deformed_positions();
    _mesh->compute_normals_and_areas();
    set_mesh_as_target();

    // Reset the constraints
    _act_vertex = nullptr;
    _valid = false;
    validate_regions();
       
    printf("[ARAP] End Deformation\n");
    printf("[ARAP] Total Iterations: %d\n", _outer_iterations);
    printf("[ARAP] Final Energy: %g\n", energy);
    printf("========================================\n");
}

// Accept the current deformed positions as the result of the solver. The
// solver must be paused. Stops the solver upon return. The actuator and
// constraints are reset.
void Deformation::accept()
{
    if (_status != Paused)
        return;

    printf("[ARAP] Did not converge (accepted)\n");

    _status = Stopped;

    // Update the mesh with the new positions
    store_deformed_positions();
    _mesh->compute_normals_and_areas();
    set_mesh_as_target();

    // Reset the constraints
    _act_vertex = nullptr;
    _valid = false;
    validate_regions();

    printf("[ARAP] End Deformation\n");
    printf("[ARAP] Total Iterations: %d\n", _outer_iterations);
    printf("[ARAP] Final Energy: %g\n", _last_energy);
    printf("========================================\n");
}

// Cancel the current deformation operation and set the positions of the
// vertices to their pre-deformation positions. The solver must must be
// paused. Stops the solvver upon return. The actuator and constraints
// are retained.
void Deformation::cancel()
{
    if (_status != Paused)
        return;

    for (Vertex *v : _mesh->vertices)
        v->position = glm::vec3(v->p_old);
    _mesh->compute_normals_and_areas();

    printf("[ARAP] Did not converge (canceled)\n");
    printf("========================================\n");

    _status = Stopped;
}

Deformation::Deformation(Mesh *mesh) :
    _mesh(mesh),
    _act_vertex(nullptr),
    _act_distance(1.0f),
    _edge_weights(Edge::UniformWeights),
    _valid(false),
    _status(Stopped),
    _outer_iterations(0),
    _last_energy(0.0)
{
    set_mesh_as_target();
}

Deformation::~Deformation()
{

}

// Computes the covariance matrix S_i = P_i * D_i * Pprime_i^T
static glm::dmat3 compute_covariance(
    const std::vector<glm::dvec3> &P_i,       // 3 x N
    const std::vector<double> &D_i,           // N x N (diagonal weights)
    const std::vector<glm::dvec3> &Pprime_i   // 3 x N
)
{
    size_t N = P_i.size();

    assert(Pprime_i.size() == N && D_i.size() == N);

    // Multiply S_i = P_i*D_i*Pprime_i^T
    glm::dmat3 S_i(0.0f);
    for (size_t k = 0; k < N; ++k)
        S_i += glm::outerProduct(P_i[k]*D_i[k], Pprime_i[k]);

    return S_i;
}

// Symmetric eigendecomposition: S = V * diag(eig) * V^T  (cyclic Jacobi)
static void jacobi_eigen_sym(glm::dmat3 S, glm::dmat3 &V, glm::dvec3 &eig)
{
    V = glm::dmat3(1.0);
    for (int sweep = 0; sweep < 32; ++sweep)
    {
        double off = S[1][0]*S[1][0] + S[2][0]*S[2][0] + S[2][1]*S[2][1];
        if (off < 1e-30)
            break;

        for (int p = 0; p < 2; ++p)
        {
            for (int q = p + 1; q < 3; ++q)
            {
                double apq = S[q][p];
                if (std::abs(apq) < 1e-300)
                    continue;
                double theta = (S[q][q] - S[p][p]) / (2.0 * apq);
                double t = (theta >= 0 ? 1.0 : -1.0) /
                        (std::abs(theta) + std::sqrt(theta*theta + 1.0));
                double c = 1.0 / std::sqrt(t*t + 1.0), s = t * c;

                glm::dmat3 J(1.0);
                J[p][p] = c;  J[q][q] = c;
                J[q][p] = s;   // row p, col q
                J[p][q] = -s;  // row q, col p

                S = glm::transpose(J) * S * J;
                V = V * J;
            }
        }
    }
    eig = glm::dvec3(S[0][0], S[1][1], S[2][2]);
}

// Returns a vector orthogonal to u
static glm::dvec3 any_orthogonal(const glm::dvec3 &u)
{
    glm::dvec3 axis = std::abs(u.x) < 0.9 ? glm::dvec3(1,0,0) : glm::dvec3(0,1,0);
    return glm::normalize(glm::cross(u, axis));
}

// A = U * diag(sigma) * transpose(V), sigma sorted descending, sigma >= 0
static void svd(const glm::dmat3 &Af, glm::dmat3 &U_out, glm::dvec3 &sigma_out, glm::dmat3 &V_out)
{
    glm::dmat3 A(Af), V;
    glm::dvec3 eig;
    jacobi_eigen_sym(glm::transpose(A) * A, V, eig);

    // Sort eigenvalues descending, permuting the columns of V to match
    for (int i = 0; i < 2; ++i)
    {
        for (int j = i + 1; j < 3; ++j)
        {
            if (eig[j] > eig[i])
                std::swap(eig[i], eig[j]); std::swap(V[i], V[j]);
        }
    }

    glm::dmat3 B = A * V;  // column i = sigma_i * u_i
    glm::dvec3 s(glm::length(B[0]), glm::length(B[1]), glm::length(B[2]));
    double eps = 1e-12 * std::max(s[0], 1e-300);

    glm::dmat3 U;
    U[0] = s[0] > eps ? B[0] / s[0] : glm::dvec3(1, 0, 0);

    if (s[1] > eps)
    {
        glm::dvec3 u1 = B[1] - glm::dot(B[1], U[0]) * U[0];  // re-orthogonalize
        U[1] = glm::normalize(u1);
    }
    else
        U[1] = any_orthogonal(U[0]);

    if (s[2] > eps)
    {
        glm::dvec3 c = glm::cross(U[0], U[1]);
        U[2] = glm::dot(c, B[2]) >= 0 ? c : -c;  // match B's direction
    }
    else
        U[2] = glm::cross(U[0], U[1]);

    U_out = glm::dmat3(U);
    V_out = glm::dmat3(V);
    sigma_out = glm::dvec3(s);
}

// Compute the best-fitting rotation matrix for a vertex
static void compute_rotation_matrix(Vertex *v_i)
{
    // Fill matrices P_i, D_i, and Pprime_i
    size_t N = v_i->edges.size();
    std::vector<glm::dvec3> P_i(N);  // 3 x N
    std::vector<double> D_i(N);  // N x N (diagonal weights)
    std::vector<glm::dvec3> Pprime_i(N);  // 3 x N
    for (size_t j = 0; j < N; ++j)
    {
        Edge *e = v_i->edges[j];
        Vertex *v_j = e->vertices[0] == v_i ? e->vertices[1] : e->vertices[0];

        // TODO: Fill the j-th entry of P_i, D_i, and Pprime_i with
        // the undeformed edge vector, edge weight, and deformed edge
        // vectors for the edge (v_j -> v_i)
        // Hint: Undeformed vertex positions are stored in Vertex::p_old,
        // edge weights in Edge::weight, and deformed vertex positions
        // in Vertex::pprime. Edge vectors is the vector pointing from
        // v_j to v_i.
        P_i[j] = v_i->p_old - v_j->p_old;
        D_i[j] = e->weight;
        Pprime_i[j] = v_i->pprime - v_j->pprime;
    }

    // Compute the covariance matrix (eq. 5)
    glm::dmat3 S_i = compute_covariance(P_i, D_i, Pprime_i);

    // Compute SVD of S_i
    glm::dmat3 U, V;
    glm::dvec3 sigma;
    svd(S_i, U, sigma, V);

    // Compute rotation matrix (eq. 6)
    v_i->R = V * glm::transpose(U);
    if (glm::determinant(v_i->R) < 0)
    {
        // Ensure positive determinant (flip col with smallest singular value)
        U[2] *= -1.0;
        v_i->R = V * glm::transpose(U);
    }
}

// Compute rotation matrices for all vertices
void Deformation::compute_rotation_matrices()
{
    for (Vertex *v : _free_vertices)
        compute_rotation_matrix(v);
    for (Vertex *v : _boundary_vertices)
        compute_rotation_matrix(v);
}

// Solve for the deformed vertex positions that best fit the rotation matrices
void Deformation::solve_vertex_positions()
{
    for (int k = 0; k < MaxInnerIterations; ++k)
    {
        for (Vertex *v_i : _free_vertices)
        {
            // Compute the RHS of the vertex solver step (eq. 8)
            glm::dvec3 rhs(0.0);
            double sum_weights = 0.0;
            for (Edge *e : v_i->edges)
            {
                Vertex *v_j = e->vertices[0] == v_i ? e->vertices[1] : e->vertices[0];

                // TODO: Accumulate the right-hand-side of equation 8 for
                // vertex v_i and its 1-ring neighbor v_j.
                // Hint: Vertex rotation matrices are stored in Vertex::R. Since
                // we are doing a Gauss-Seidel solver, treat the deformed positions
                // of *neighboring* vertices (p'_j) as constant by rearanging the
                // left-hand-side of the equation. It may be helpful to rearrange
                // by hand first before programming.
                rhs += e->weight * (v_j->pprime + 0.5 * (v_i->R + v_j->R) * (v_i->p_old - v_j->p_old));
                sum_weights += e->weight;
            }

            // TODO: Update v_i->pprime using the accumulated right-hand-side
            // vector.
            // Hint: Normalize by the sum of the edge weights if needed.
            v_i->pprime = rhs / sum_weights;
        }
    }
}

// Store the current deformed positions as the vertex positions to be rendered
void Deformation::store_deformed_positions()
{
    for (Vertex *v : _free_vertices)
        v->position = glm::vec3(v->pprime);
}

// Compute the ARAP energy
double Deformation::compute_energy()
{
    // Compute the energy of a single vertex
    const auto vertex_energy = [this](Vertex *v_i) {
        double E = 0.0;
        const glm::dvec3 &pprime_i = v_i->pprime;
        for (Edge *e : v_i->edges)
        {
            Vertex *v_j = e->vertices[0] == v_i ? e->vertices[1] : e->vertices[0];
            const glm::dvec3 &pprime_j = v_j->pprime;
            glm::dvec3 e_ij = v_i->p_old - v_j->p_old;
            glm::dvec3 eprime_ij = pprime_i - pprime_j;
            E += e->weight * glm::length2(eprime_ij - v_i->R*e_ij);
        }
        return E;
    };

    // The energy is define for all vertices in the mesh, not just the free
    // vertices. The energy is only guaranteed to decrease if computed
    // over the whole mesh. Note that constrained vertices far from the boundary
    // will always have energy 0, so they are skipped.
    double E = 0.0;
    for (Vertex *v_i : _free_vertices)
        E += vertex_energy(v_i);
    for (Vertex *v_i : _boundary_vertices)
        E += vertex_energy(v_i);
    return E;
}
