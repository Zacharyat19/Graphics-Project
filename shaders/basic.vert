#version 120

varying vec3 vColor;

void main()
{
    vColor = gl_Color.rgb;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
