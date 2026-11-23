#version 120

uniform float L;

varying vec3 vObjectPos;

vec3 mapper(vec3 v)
{
    return 1.0 - mod(v, vec3(2.0));
}

void main()
{
    gl_FragColor = vec4(mapper(floor(vObjectPos / L)), 1.0);
}
