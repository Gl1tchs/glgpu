#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 out_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D h_textures[];
layout(set = 0, binding = 1) uniform texture2D h_sampled_images[];
layout(set = 0, binding = 2) uniform sampler h_samplers[];
layout(set = 0, binding = 3, rgba8) uniform writeonly image2D h_storage_images[];
layout(set = 0, binding = 4) buffer h_buffers { vec4 data[]; }
h_storage_buffers[];

void main() {
	vec4 c = texture(h_textures[nonuniformEXT(0)], out_uv);
	c += texture(
			sampler2D(h_sampled_images[nonuniformEXT(0)], h_samplers[nonuniformEXT(0)]), out_uv);
	imageStore(h_storage_images[nonuniformEXT(0)], ivec2(0), vec4(1.0));
	c += h_storage_buffers[nonuniformEXT(0)].data[0];
	out_color = c;
}
