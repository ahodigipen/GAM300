#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

// Per-instance render data from compute shader: 2 vec4s per particle
// [posX, posY, posZ, size] [r, g, b, a]
layout(std430, binding = 0) buffer RenderBuffer {
    vec4 renderData[];
};

uniform mat4 uViewProj;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform int  uBillboard;

out vec2 vUV;
out vec4 vColor;

void main() {
    uint base = gl_InstanceID * 2u;
    vec4 posSize = renderData[base + 0u];
    vec4 color   = renderData[base + 1u];

    vec3 particlePos = posSize.xyz;
    float size       = posSize.w;

    vUV    = aUV;
    vColor = color;

    vec3 worldPos;
    if (uBillboard != 0) {
        // Billboard: construct world position from camera-aligned quad
        worldPos = particlePos
                 + uCamRight * (aPos.x * size)
                 + uCamUp    * (aPos.y * size);
    } else {
        worldPos = particlePos + aPos * size;
    }

    gl_Position = uViewProj * vec4(worldPos, 1.0);
}
==VERTEX==

#version 450 core

in vec2 vUV;
in vec4 vColor;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 out_brightness;

void main() {
    // Procedural soft circle (no texture needed)
    vec2 center = vUV - vec2(0.5);
    float dist = length(center) * 2.0;
    float alpha = 1.0 - smoothstep(0.6, 1.0, dist);

    // Apply particle color from compute shader (already interpolated)
    FragColor = vec4(vColor.rgb, vColor.a * alpha);

    if (FragColor.a < 0.01) discard;

    // Bloom output: bright particles contribute to bloom
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 0.5)
        out_brightness = vec4(FragColor.rgb * FragColor.a, FragColor.a);
    else
        out_brightness = vec4(0.0, 0.0, 0.0, FragColor.a);
}
==FRAGMENT==
