#version 460

#extension GL_ARB_shading_language_include : require
#include "DnmGLLite.glsl"

const vec2 vertices[] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.0,  0.5),
    vec2( 0.5, -0.5)
);

const vec2 uv[] = vec2[](
    vec2( 0.0,  0.0),
    vec2( 0.5,  1.0),
    vec2( 1.0,  0.0)
);

layout(location = 0) out vec2 out_uv;

void main() {
    gl_Position = vec4(-vertices[gl_VertexIndex], 0, 1);
    out_uv = uv[gl_VertexIndex];
}
