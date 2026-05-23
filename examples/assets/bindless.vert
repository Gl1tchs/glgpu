#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in int in_tex_index;

layout(location = 0) out vec2 out_uv;
layout(location = 1) flat out int out_tex_index;

void main() {
	gl_Position = vec4(in_pos, 0.0, 1.0);
	out_uv = in_uv;
	out_tex_index = in_tex_index;
}
