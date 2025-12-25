#version 460

#extension GL_ARB_shading_language_include : require
#include "DnmGLLite.glsl"

layout(location = 0) out vec4 frag_color;
layout(location = 0) in in_block {
    vec2 vert_pos;
    vec2 uv;
    flat vec4 color;
    flat float color_factor;
} in_;

Texture2D(0, 1) atlas_texture;

void main() {
    frag_color = mix(textureLod(atlas_texture, in_.uv, 0), in_.color, in_.color_factor);
}

