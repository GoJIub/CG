#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 model;
    mat4 view;
    mat4 projection;
} globals;

layout(location = 0) out vec3 out_color;

void main() {
    gl_Position =
        globals.projection *
        globals.view *
        globals.model *
        vec4(in_position, 1.0);

    out_color = in_color;
}