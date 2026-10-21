#version 450

layout(location = 0) in vec3 in_position;

layout(std140, set = 0, binding = 0) uniform SceneUniformData {
    mat4 view_projection;
} scene_data;

layout(push_constant) uniform MarkerPushConstants {
    mat4 model;
    vec4 color;
} marker;

layout(location = 0) out vec3 out_color;

void main() {
    gl_Position =
        scene_data.view_projection *
        marker.model *
        vec4(in_position, 1.0);

    out_color = marker.color.rgb;
}