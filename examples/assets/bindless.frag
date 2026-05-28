#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 out_uv;
layout(location = 1) flat in int out_tex_index;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D textures[];

void main() { out_color = texture(textures[nonuniformEXT(out_tex_index)], out_uv); }
