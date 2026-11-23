#pragma once

#include <vector>

#include "edge.h"

class Mesh;
class Vertex;

// Implements a ARAP deformation using Gauss-Seidel to solve for vertex positions
// Paper: https://igl.ethz.ch/projects/ARAP/arap_web.pdf
class Deformation
{
public:
    static constexpr int MaxIterations = 5000;  // Maximum outer solver iterations
    static constexpr int MaxInnerIterations = 64;  // Maximum inner solver iterations

    static constexpr double MinAbsError = 1e-8;  // Minimum absolute error for convergence
    static constexpr double MinDeltaError = 1e-9;  // Minimum absolute delta error for convergence
    static constexpr double MinRelativeError = 1e-7;  // Minimum relative error for convergence

    static constexpr double MaxIterationTime = 1.0/60.0;  // Maximum time allowed time for an iteration before pausing

    enum Status
    {
        Stopped,  // Solver is not running
        Paused,   // Solver was started but not finished
        Running   // Solver is running
    };

    // Set the actuator vertex
    bool set_actuator(Vertex *vertex);
    inline Vertex *actuator() const { return _act_vertex; }

    // Set the acuation distance
    bool set_actuation_distance(float distance);
    inline float actuation_distance() const { return _act_distance; }
    
    // Sets the type of edge weights to use
    bool set_weight_type(Edge::Weights weights);
    inline Edge::Weights weight_type() const { return _edge_weights; }

    // Validate the free/constraint/boundary regions
    void validate_regions();

    // Set the current deformed mesh position as the undeformed positions
    void set_mesh_as_target();

    inline const std::vector<Vertex *> &free_vertices() const { return _free_vertices; }
    inline const std::vector<Vertex *> &boundary_vertices() const { return _boundary_vertices; }
    inline const std::vector<Vertex *> &constrained_vertices() const { return _constrained_vertices; }

    void solve();
    inline Status status() const { return _status; }

    void accept();
    void cancel();

    Deformation(Mesh *mesh);
    ~Deformation();
private:
    Mesh *_mesh;  // the mesh to deform

    Vertex *_act_vertex;  // actuation vertex
    float _act_distance;  // actuation distance
    Edge::Weights _edge_weights;  // edge weights to use

    bool _valid;  // Whether the free vertices are valid
    std::vector<Vertex *> _free_vertices;  // Optimizable vertices
    std::vector<Vertex *> _boundary_vertices;  // Constrained vertices in the 1-ring of an optimizable vertex
    std::vector<Vertex *> _constrained_vertices;  // Constrained vertices

    // Solver information

    Status _status;  // Status
    int _outer_iterations;  // Total number of outer iterations ran
    double _last_energy;  // Energy of last iteration

    void compute_rotation_matrices();
    void solve_vertex_positions();

    void store_deformed_positions();
    double compute_energy();
};
