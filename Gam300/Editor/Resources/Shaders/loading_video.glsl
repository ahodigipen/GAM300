#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 in_uv;
layout(location = 1) out vec2 out_uv;
uniform mat4 uProj;

void main() {
    out_uv = in_uv;
    gl_Position = uProj * vec4(pos, 0.0, 1.0);
}
==VERTEX==

#version 450 core

layout(location = 1) in vec2 uvs;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for video)
uniform sampler2D videoTex;
uniform vec4 tintColor;
uniform float brightness;

void main() {
    vec4 videoColor = texture(videoTex, uvs).rgba;

    if (videoColor.a < 0.01) discard;

    FragColor = vec4((tintColor * videoColor).rgb * brightness, (tintColor * videoColor).a);
    out_brightness = vec4(0.0, 0.0, 0.0, 1.0);  // Video should NOT contribute to bloom
}
==FRAGMENT==
