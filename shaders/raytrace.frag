#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform mat4 uInvViewProj;

void main()
{
    vec4 near = uInvViewProj * vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 far = uInvViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);

    vec3 origin = near.xyz / near.w;
    vec3 dir = normalize(far.xyz / far.w - origin);

    frag_color = vec4(dir * 0.5 + 0.5, 1.0);
}
