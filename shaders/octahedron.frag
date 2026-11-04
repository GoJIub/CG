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

struct SpotLightData {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 cone_cosines;
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLights {
    SpotLightData lights[];
} spot_light_buffer;

layout(std140, set = 1, binding = 0) uniform ObjectUniformData {
    mat4 model;
    vec4 albedo;
    vec4 specular_shininess;
    vec4 emissive_color_intensity;
} object_data;

layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D specular_texture;
layout(set = 1, binding = 3) uniform sampler2D emissive_texture;

layout(push_constant) uniform TexturePushConstants {
    float time;
    uint animation_enabled;
} texture_effect;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_world_position;

layout(location = 0) out vec4 out_color;

vec3 evaluate_blinn_phong(
    vec3 N,
    vec3 L,
    vec3 V,
    vec3 albedo,
    vec3 specular_color
) {
    float diffuse_factor = max(dot(N, L), 0.0);

    if (diffuse_factor <= 0.0) {
        return vec3(0.0);
    }

    vec3 diffuse = albedo * diffuse_factor;

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

    vec3 specular = specular_color * specular_factor;

    return diffuse + specular;
}

vec3 evaluate_point_light(
    PointLightData light,
    vec3 N,
    vec3 V,
    vec3 albedo,
    vec3 specular_color
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
        evaluate_blinn_phong(
            N, L, V, albedo, specular_color
        );
}

vec3 evaluate_spot_light(
    SpotLightData light,
    vec3 N,
    vec3 V,
    vec3 albedo,
    vec3 specular_color
) {
    vec3 from_light = in_world_position - light.position.xyz;
    float distance_squared = dot(from_light, from_light);

    const float minimum_direction_distance_squared = 1e-12;

    if (distance_squared <= minimum_direction_distance_squared) {
        return vec3(0.0);
    }

    vec3 Q = from_light / sqrt(distance_squared);

    float cos_angle = clamp(
        dot(light.direction.xyz, Q),
        -1.0,
        1.0
    );

    float cos_inner = light.cone_cosines.x;
    float cos_outer = light.cone_cosines.y;

    float cone_factor;

    if (cos_inner > cos_outer) {
        cone_factor = smoothstep(
            cos_outer,
            cos_inner,
            cos_angle
        );
    } else {
        cone_factor = step(cos_outer, cos_angle);
    }

    if (cone_factor <= 0.0) {
        return vec3(0.0);
    }

    PointLightData point_light = PointLightData(
        light.position,
        light.color
    );

    return cone_factor * evaluate_point_light(
        point_light,
        N,
        V,
        albedo,
        specular_color
    );
}

void main() {
    vec3 albedo =
        texture(albedo_texture, in_uv).rgb *
        object_data.albedo.rgb;

    vec3 specular_color =
        texture(specular_texture, in_uv).rgb *
        object_data.specular_shininess.rgb;

    vec3 emissive =
        texture(emissive_texture, in_uv).rgb *
        object_data.emissive_color_intensity.rgb *
        object_data.emissive_color_intensity.w;

    if (texture_effect.animation_enabled != 0u) {
        float time = texture_effect.time;

        vec2 flow_uv_a =
            in_uv * 1.7 + vec2(0.06, -0.04) * time;

        vec2 flow_uv_b =
            in_uv * 2.3 + vec2(-0.03, 0.05) * time;

        float flow_a = texture(
            emissive_texture,
            flow_uv_a
        ).r;

        float flow_b = texture(
            emissive_texture,
            flow_uv_b
        ).r;

        float distortion = clamp(
            2.0 * (flow_a + flow_b),
            0.0,
            1.0
        );

        const float wave_frequency = 12.0;
        const float wave_speed = 2.5;

        float phase =
            dot(in_uv, vec2(1.0, 0.65)) * wave_frequency -
            time * wave_speed +
            distortion * 2.5;

        float wave = smoothstep(
            0.25,
            0.9,
            sin(phase)
        );

        float brightness = mix(0.05, 3.0, wave);

        vec3 glow_tint = mix(
            vec3(1.0, 0.22, 0.04),
            vec3(1.0, 0.95, 0.7),
            wave
        );

        emissive *= brightness * glow_tint;
    }

    vec3 N = normalize(in_normal);

    vec3 L = normalize(
        -scene_data.directional_light_direction.xyz
    );

    vec3 V = normalize(
        scene_data.camera_position.xyz - in_world_position
    );

    vec3 ambient =
        scene_data.ambient_light.rgb * albedo;

    vec3 directional =
        scene_data.directional_light_color.rgb *
        evaluate_blinn_phong(
            N, L, V, albedo, specular_color
        );

    vec3 result = ambient + directional;

    for (uint i = 0u; i < scene_data.light_counts.x; ++i) {
        result += evaluate_point_light(
            point_light_buffer.lights[i],
            N,
            V,
            albedo,
            specular_color
        );
    }

    for (uint i = 0u; i < scene_data.light_counts.y; ++i) {
        result += evaluate_spot_light(
            spot_light_buffer.lights[i],
            N,
            V,
            albedo,
            specular_color
        );
    }

    out_color = vec4(result + emissive, 1.0);
}
