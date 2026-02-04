#version 450 core
layout (location = 0) in vec3 position;

out vec3 worldPosition;

uniform mat4 proj;
uniform mat4 view;

void main() {
    gl_Position = proj * view * vec4(position, 1.0);
    worldPosition = normalize(position);
}
==VERTEX==

#version 450 core
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 out_brightness;  // Brightness for bloom (always zero for skymap)

in vec3 worldPosition;

uniform sampler2D map;

//converts 3D spherical coord to 2D texture coord
//equirectangular mapping
vec2 GetSphericalUV(vec3 dir) {
    float theta = atan(dir.z, dir.x);
    float phi   = atan(length(dir.xz), dir.y);

    vec2 uv;
    uv.x = theta * 0.15915494309189533577 + 0.5;
    uv.y = 1.0 - (phi   * 0.31830988618379067154);

    return uv;
}

void main() {
    fragColor = texture(map, GetSphericalUV(worldPosition));
    out_brightness = vec4(0.0, 0.0, 0.0, 1.0);  // Skymap should NOT contribute to bloom
}
==FRAGMENT==