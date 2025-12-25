#version 460

#extension GL_ARB_shading_language_include : require
#include "DnmGLLite.glsl"

layout(location = 0) out vec4 frag_color;

Texture2D(0, 0) tex;

layout(location = 0) in vec2 in_uv;

void main() {
    frag_color = textureLod(tex, in_uv, 5);
}
