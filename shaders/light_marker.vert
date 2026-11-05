#version 450

layout(location = 0) in vec3 in_position;

layout(std140, set = 0, binding = 0) uniform SceneUniformData {
    mat4 view_projection;
} scene_data;

layout(std140, set = 1, binding = 0) uniform ObjectUniformData {
    mat4 model;
    vec4 albedo;
    vec4 specular_shininess;
    vec4 emissive_color_intensity;
} object_data;

layout(location = 0) out vec3 out_color;

void main() {
    gl_Position =
        scene_data.view_projection *
        object_data.model *
        vec4(in_position, 1.0);

    out_color = object_data.albedo.rgb;
}