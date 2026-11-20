#version 450

layout(location = 0) in vec3 in_position;

layout(std140, set = 0, binding = 0) uniform ShadowUniformData {
    mat4 light_view_projection;
} shadow_data;

layout(std140, set = 1, binding = 0) uniform ObjectUniformData {
    mat4 model;
} object_data;

void main() {
    gl_Position = shadow_data.light_view_projection *
                  object_data.model * vec4(in_position, 1.0);
}