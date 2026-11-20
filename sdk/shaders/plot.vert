#version 440

layout(location = 0) in vec2 position;   // device pixels, origin at the top-left corner
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;                            // pixel coordinates -> clip space (backend corrected)
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vColor = color;
    gl_Position = ubuf.mvp * vec4(position, 0.0, 1.0);
}
