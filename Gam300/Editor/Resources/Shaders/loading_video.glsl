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
out vec4 FragColor;
uniform sampler2D videoTex;
uniform vec4 tintColor;

void main() {
    vec4 videoColor = texture(videoTex, uvs).rgba;

    if (videoColor.a < 0.01) discard;

    FragColor = tintColor * videoColor;
}
==FRAGMENT==
