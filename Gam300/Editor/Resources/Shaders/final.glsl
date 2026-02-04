
#version 450 core
layout(location = 0) in vec2 pos;     // matches QuadVert.pos
layout(location = 1) in vec2 in_uv;   // matches QuadVert.uv

out vec2 uvs;

void main() {
    uvs = in_uv;
    gl_Position = vec4(pos, 0.0, 1.0);
}

==VERTEX==

#version 450 core
out vec4 out_fragment;
in vec2 uvs;

const float GAMMA = 1;
const float EXPOSURE = 4;
const float MIN_GAMMA = 0.000001;

uniform sampler2D map;
uniform sampler2D u_bloom;
uniform bool u_enableBloom;
uniform float u_fadeAlpha; // NEW: Fade to black alpha
uniform float u_bloomIntensity = 1.0; // Bloom intensity multiplier

void main()
{
  vec3 result = texture(map, uvs).rgb;

  if (u_enableBloom) {
      result += texture(u_bloom, uvs).rgb * u_bloomIntensity;
  }

  // gamma correction
  result = pow(result, vec3(GAMMA));
  result = vec3(1.0) - exp(-result * EXPOSURE); 
  result = pow(result, vec3(1.0 / max(GAMMA, MIN_GAMMA)));

  // Apply fade to black
  result = mix(result, vec3(0.0), u_fadeAlpha);

  out_fragment = vec4(result, 1.0); 
}

==FRAGMENT==