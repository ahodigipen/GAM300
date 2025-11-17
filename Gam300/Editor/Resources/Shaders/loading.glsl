#version 450 core

layout(location = 0) in vec2 pos;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * vec4(pos, 0.0, 1.0);
}
==VERTEX==

#version 450 core

uniform vec4 color;
out vec4 FragColor;

void main() {
    FragColor = color;
}
==FRAGMENT==