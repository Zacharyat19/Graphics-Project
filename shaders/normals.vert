#version 120

varying vec3 vNormal;

void main()
{
    vNormal = gl_Normal;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
