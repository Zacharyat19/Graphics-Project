#version 330 core

// Runs once per pixel. The ray tracing goes here.

in vec2 uv;
out vec4 frag_color;

void main()
{
    // Placeholder: a solid color, so we can see the quad covers the window.
    frag_color = vec4(0.2, 0.3, 0.5, 1.0);
}
