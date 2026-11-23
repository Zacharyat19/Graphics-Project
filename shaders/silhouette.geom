#version 150

layout (triangles_adjacency) in;
layout (line_strip, max_vertices = 6) out;

bool facesCamera(vec3 a, vec3 b, vec3 c)
{
    return cross(b - a, c - a).z > 0.0;
}

void emitEdge(int i0, int i1)
{
    gl_Position = gl_in[i0].gl_Position;
    EmitVertex();

    gl_Position = gl_in[i1].gl_Position;
    EmitVertex();

    EndPrimitive();
}

void main()
{
    vec3 p0 = gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w;
    vec3 p1 = gl_in[1].gl_Position.xyz / gl_in[1].gl_Position.w;
    vec3 p2 = gl_in[2].gl_Position.xyz / gl_in[2].gl_Position.w;
    vec3 p3 = gl_in[3].gl_Position.xyz / gl_in[3].gl_Position.w;
    vec3 p4 = gl_in[4].gl_Position.xyz / gl_in[4].gl_Position.w;
    vec3 p5 = gl_in[5].gl_Position.xyz / gl_in[5].gl_Position.w;

    bool test = facesCamera(p0, p2, p4);

    if (facesCamera(p0, p1, p2) != test)
        emitEdge(0, 2);
        
    if (facesCamera(p2, p3, p4) != test)
        emitEdge(2, 4);
        
    if (facesCamera(p4, p5, p0) != test)
        emitEdge(4, 0);
}
