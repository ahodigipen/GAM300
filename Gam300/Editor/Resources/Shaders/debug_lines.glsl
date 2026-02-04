#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;

uniform mat4 u_View;
uniform mat4 u_Proj;

out vec4 vCol;

void main()
{
    gl_Position = u_Proj * u_View * vec4(aPos, 1.0);
    vCol = aCol;
}
==VERTEX==

#version 450 core
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for debug lines)

in vec4 vCol;

void main()
{
    fragColor = vCol;
    out_brightness = vec4(0.0, 0.0, 0.0, 1.0);  // Debug lines should NOT contribute to bloom
}
==FRAGMENT==