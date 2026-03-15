#version 450 core

// Per-vertex quad data
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

// Per-instance data from SSBO (binding 5)
struct ParticleInstance {
    mat4  model;
    vec4  color;
};

layout(std430, binding = 5) buffer ParticleBuffer {
    ParticleInstance instances[];
};

uniform mat4 uViewProj;

out vec2 vUV;
out vec4 vColor;

void main() {
    ParticleInstance inst = instances[gl_InstanceID];

    vUV    = aUV;
    vColor = inst.color;

    vec4 worldPos = inst.model * vec4(aPos, 1.0);
    gl_Position   = uViewProj * worldPos;
}
==VERTEX==

#version 450 core

in vec2 vUV;
in vec4 vColor;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 out_brightness;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vUV);

    // If no texture bound (ID 0), use a soft circular falloff
    if (texColor.a < 0.01 && texColor.r < 0.01) {
        // Procedural soft circle
        vec2 center = vUV - vec2(0.5);
        float dist = length(center) * 2.0;
        float alpha = 1.0 - smoothstep(0.0, 1.0, dist);
        texColor = vec4(1.0, 1.0, 1.0, alpha);
    }

    FragColor = vColor * texColor;

    if (FragColor.a < 0.01) discard;

    // Bloom output: bright particles contribute to bloom
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 0.8)
        out_brightness = vec4(FragColor.rgb * 0.5, FragColor.a);
    else
        out_brightness = vec4(0.0, 0.0, 0.0, FragColor.a);
}
==FRAGMENT==
