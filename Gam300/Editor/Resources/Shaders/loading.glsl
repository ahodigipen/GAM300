#version 450 core

layout(location = 0) in vec2 pos;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * vec4(pos, 0.0, 1.0);
}
==VERTEX==

#version 450 core

uniform vec4 color;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for loading screen)

void main() {
    FragColor = color;
    out_brightness = vec4(0.0, 0.0, 0.0, 1.0);  // Loading screen should NOT contribute to bloom
}
==FRAGMENT==