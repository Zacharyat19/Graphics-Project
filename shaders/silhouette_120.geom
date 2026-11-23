#version 120

#extension GL_EXT_geometry_shader4 : enable

bool facesCamera(vec3 a, vec3 b, vec3 c)
{
    return cross(b - a, c - a).z > 0.0;
}

void emitEdge(int i0, int i1)
{
    gl_Position = gl_PositionIn[i0];
    EmitVertex();

    gl_Position = gl_PositionIn[i1];
    EmitVertex();

    EndPrimitive();
}

void main()
{
    vec3 p0 = gl_PositionIn[0].xyz / gl_PositionIn[0].w;
    vec3 p1 = gl_PositionIn[1].xyz / gl_PositionIn[1].w;
    vec3 p2 = gl_PositionIn[2].xyz / gl_PositionIn[2].w;
    vec3 p3 = gl_PositionIn[3].xyz / gl_PositionIn[3].w;
    vec3 p4 = gl_PositionIn[4].xyz / gl_PositionIn[4].w;
    vec3 p5 = gl_PositionIn[5].xyz / gl_PositionIn[5].w;

    bool test = facesCamera(p0, p2, p4);

    if (facesCamera(p0, p1, p2) != test)
        emitEdge(0, 2);
        
    if (facesCamera(p2, p3, p4) != test)
        emitEdge(2, 4);
        
    if (facesCamera(p4, p5, p0) != test)
        emitEdge(4, 0);
}
