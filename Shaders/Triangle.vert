#version 460

#extension GL_ARB_shading_language_include : require
#include "DnmGLLite.glsl"

vec2 vertices[3] = {
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
};

const vec3 colors[3] = {
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1)
};

layout(location = 0) out vec3 color;

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0, 1);
    color = colors[gl_VertexIndex];
}
