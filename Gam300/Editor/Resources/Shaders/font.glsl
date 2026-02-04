#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
==VERTEX==
#version 450 core
in vec2 TexCoord;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for text)

uniform sampler2D text;
uniform vec3 textColor;
uniform float textAlpha;

void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoord).r);
    color = vec4(textColor, textAlpha) * sampled;
    out_brightness = vec4(0.0, 0.0, 0.0, 1.0);  // Text should NOT contribute to bloom
}
==FRAGMENT==
