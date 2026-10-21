#version 450

layout(std140, set = 0, binding = 0) uniform SceneUniformData {
    mat4 view_projection;
    vec4 camera_position;

    vec4 directional_light_direction;
    vec4 directional_light_color;
    vec4 ambient_light;

    uvec4 light_counts;
} scene_data;

struct PointLightData {
    vec4 position;
    vec4 color;
};

layout(std430, set = 0, binding = 1) readonly buffer PointLights {
    PointLightData lights[];
} point_light_buffer;

layout(std140, set = 1, binding = 0) uniform ObjectUniformData {
    mat4 model;
    vec4 albedo;
    vec4 specular_shininess;
} object_data;

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_world_position;

layout(location = 0) out vec4 out_color;

vec3 evaluate_blinn_phong(vec3 N, vec3 L, vec3 V) {
    float diffuse_factor = max(dot(N, L), 0.0);

    if (diffuse_factor <= 0.0) {
        return vec3(0.0);
    }

    vec3 diffuse = object_data.albedo.rgb * diffuse_factor;

    float specular_factor = 0.0;

    vec3 half_sum = L + V;

    const float minimum_half_vector_length_squared = 1e-8;

    if (dot(half_sum, half_sum) >
        minimum_half_vector_length_squared) {
        vec3 H = normalize(half_sum);

        float shininess = object_data.specular_shininess.w;

        specular_factor = pow(
            max(dot(N, H), 0.0),
            shininess
        );
    }

    vec3 specular =
        object_data.specular_shininess.rgb * specular_factor;

    return diffuse + specular;
}

vec3 evaluate_point_light(
    PointLightData light,
    vec3 N,
    vec3 V
) {
    vec3 to_light = light.position.xyz - in_world_position;

    float distance_squared = dot(to_light, to_light);

    const float minimum_direction_distance_squared = 1e-12;

    if (distance_squared <= minimum_direction_distance_squared) {
        return vec3(0.0);
    }

    vec3 L = to_light / sqrt(distance_squared);

    const float minimum_attenuation_distance = 0.01;

    float attenuation = 1.0 / max(
        distance_squared,
        minimum_attenuation_distance *
            minimum_attenuation_distance
    );

    return light.color.rgb *
        attenuation *
        evaluate_blinn_phong(N, L, V);
}

void main() {
    vec3 N = normalize(in_normal);

    vec3 L = normalize(
        -scene_data.directional_light_direction.xyz
    );

    vec3 V = normalize(
        scene_data.camera_position.xyz - in_world_position
    );

    vec3 ambient =
        scene_data.ambient_light.rgb * object_data.albedo.rgb;

    vec3 directional =
        scene_data.directional_light_color.rgb *
        evaluate_blinn_phong(N, L, V);

    vec3 result = ambient + directional;

    for (uint i = 0u; i < scene_data.light_counts.x; ++i) {
        result += evaluate_point_light(
            point_light_buffer.lights[i],
            N,
            V
        );
    }

    out_color = vec4(result, 1.0);
}