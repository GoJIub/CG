#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;

layout(std140, set = 0, binding = 0) uniform SceneUniformData {
    mat4 view_projection;
    vec4 camera_position;

    vec4 directional_light_direction;
    vec4 directional_light_color;
    vec4 ambient_light;

    uvec4 light_counts;
} scene_data;

layout(std140, set = 1, binding = 0) uniform ObjectUniformData {
    mat4 model;
    vec4 albedo;
    vec4 specular_shininess;
} object_data;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_world_position;

void main() {
    vec4 world_position =
        object_data.model * vec4(in_position, 1.0);

    out_world_position = world_position.xyz;

    gl_Position =
        scene_data.view_projection * world_position;

    out_color = in_color;

    mat3 normal_matrix =
        transpose(inverse(mat3(object_data.model)));
    out_normal = normalize(normal_matrix * in_normal);
}