
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

void main() 
{ 
  
  vec3 result = texture(map, uvs).rgb;
  // sample color from map
    if (u_enableBloom) {
        result += texture(u_bloom, uvs).rgb;
    }

  // gamma correction
  result = pow(result, vec3(GAMMA));
  result = vec3(1.0) - exp(-result * EXPOSURE); 
  result = pow(result, vec3(1.0 / max(GAMMA, MIN_GAMMA)));

  // fragment color
  out_fragment = vec4(result, 1.0); 
/*
  //depth map
  float depth = texture(map, uvs).r;
  out_fragment = vec4(depth, depth, depth, 1.0);
*/
}

==FRAGMENT==