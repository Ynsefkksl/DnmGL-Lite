#version 460

#extension GL_ARB_shading_language_include : require
#include "DnmGLLite.glsl"

const vec2 pos[4] = vec2[](
    vec2(-1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0)
);

const ivec2 uv[4] = ivec2[](
    ivec2(0, 1),
    ivec2(0, 0),
    ivec2(1, 1),
    ivec2(1, 0)
);

struct SpriteData {
    vec4 color;
    vec4 sprite_coords;
    vec2 pos;
    vec2 scale;
    float angle;
    float color_factor;
};

StorageBuffer(0, 0) restrict readonly spriteBuffer {
    SpriteData sprite_data[];
};

PushConstant ps {
    mat4 proj_mtx;
};

layout(location = 0) out outBlock {
    vec2 vert_pos;
    vec2 uv;
    flat vec4 color;
    flat float color_factor;
} out_;

mat3 GetModelMtx(const vec2 pos, const vec2 scale, const float angle) {
    const mat3 scale_mtx = mat3(
        vec3(scale.x, 0, 0),
        vec3(0, scale.y, 0),
        vec3(0, 0, 1)
    );

    const mat3 translation_mtx = mat3(
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(pos.x, pos.y, 1)
    );

    const mat3 rotation_mtx = mat3(
        vec3(cos(angle), sin(angle), 0),
        vec3(-sin(angle), cos(angle), 0),
        vec3(0, 0, 1)
    );

    return translation_mtx * rotation_mtx * scale_mtx;
}

void main() {
    const SpriteData sprite_data = sprite_data[gl_InstanceIndex];
    out_.color = sprite_data.color;
    out_.color_factor = sprite_data.color_factor;

    const mat3 modelMtx = GetModelMtx(sprite_data.pos, sprite_data.scale, sprite_data.angle);
    out_.vert_pos = (modelMtx * vec3(pos[gl_VertexIndex], 1)).xy;
    out_.uv = vec2(
        sprite_data.sprite_coords[uv[gl_VertexIndex].x * 2],
        sprite_data.sprite_coords[uv[gl_VertexIndex].y * 2 + 1]
        );
    gl_Position = proj_mtx * vec4(out_.vert_pos, 0, 1);
}

/*
    sprite_coords.xy = bottom left
    sprite_coords.zw = up right

    if (gl_VertexID == 0) {
        uv = (sprite_coords.x, sprite_coords.w);
    }
    if (gl_VertexID == 1) {
        uv = (sprite_coords.x, sprite_coords.y);
    }
    if (gl_VertexID == 2) {
        uv = (sprite_coords.z, sprite_coords.w);
    }
    if (gl_VertexID == 3) {
        uv = (sprite_coords.z, sprite_coords.y);
    }

    uv perfectly matched
    x = 0, z = 1 for uv.x value
    y = 0, w = 1 for uv.y value
*/