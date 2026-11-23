#version 120

varying vec3 vObjectPos;

void main()
{
    vObjectPos = vec3(gl_Vertex);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
