#version 120

varying vec3 vFragPos;
varying vec3 vNormal;

void main()
{
    vFragPos = vec3(gl_ModelViewMatrix * gl_Vertex);
    vNormal = gl_NormalMatrix * gl_Normal;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
