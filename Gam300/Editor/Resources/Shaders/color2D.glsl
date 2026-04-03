#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 in_uv;
layout(location = 1) out vec2 out_uv;
uniform mat3 mat;

void main() {
    out_uv = vec2(in_uv.x, 1.0 - in_uv.y);
    vec3 res = mat * vec3(pos, 1.0);
    gl_Position = vec4(res.x, res.y, 0.0, 1.0);
}
==VERTEX==

#version 450 core

layout(location = 1) in vec2 uvs;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for 2D GUI)
uniform vec4 color;
uniform sampler2D texMap;
uniform bool u_applyToneMap;
uniform float u_gamma;
void main() {
    vec4 result = texture(texMap, uvs).rgba;

    if (result.a < 0.01) discard;

    FragColor = color * result;

    if (u_applyToneMap) {
        const float EXPOSURE = 4.0;
        FragColor.rgb = vec3(1.0) - exp(-FragColor.rgb * EXPOSURE);
    }

    float gammaScale = pow(0.5, (1.0 / u_gamma) - (1.0 / 2.2));
    FragColor.rgb *= gammaScale;

    out_brightness = vec4(0.0, 0.0, 0.0, FragColor.a);
}
==FRAGMENT==