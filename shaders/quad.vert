#version 330 core

// Full-screen quad drawn as a 4-vertex triangle strip with no vertex buffer.
// gl_VertexID 0..3 maps to the corners (-1,-1), (1,-1), (-1,1), (1,1).

out vec2 uv;  // 0..1 across the screen

void main()
{
    vec2 pos = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
    uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
