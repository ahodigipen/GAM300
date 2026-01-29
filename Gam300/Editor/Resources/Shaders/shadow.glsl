#version 450 core
layout (location = 0) in vec3 a_position;
layout (location = 2) in vec2 a_uv;
layout (location = 5) in ivec4 joints;
layout (location = 6) in vec4 weights;

uniform mat4 u_lightSpace;
uniform mat4 u_model;

#define MAX_WEIGHTS 4
#define MAX_JOINTS 100

uniform mat4 jointsMat[MAX_JOINTS];
uniform bool hasJoints = false;

out vec2 v_uv;

void main()
{
  mat4 transform = mat4(1.0);
  if(hasJoints)
  {
      transform = mat4(0.0);
      for(int i = 0; i < MAX_WEIGHTS && joints[i] > -1; i++)
      {
          transform += jointsMat[joints[i]] * weights[i];
      }
  }
  v_uv = a_uv;
  gl_Position = u_lightSpace * u_model * transform * vec4(a_position, 1.0f);
}

==VERTEX==

#version 450 core

in vec2 v_uv;

uniform float u_opacity = 1.0;
uniform bool u_hasOpacityMap = false;
uniform sampler2D u_opacityMap;

// 4x4 Bayer dither matrix for stochastic shadow discarding
const float bayerMatrix[16] = float[16](
    0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
   12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
    3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
   15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
);

void main()
{
  // Get opacity from map or uniform
  float opacity = u_opacity;
  if (u_hasOpacityMap) {
      opacity *= texture(u_opacityMap, v_uv).r;
  }

  // Stochastic discarding based on opacity using dither pattern
  // This creates softer shadows for transparent objects
  if (opacity < 1.0) {
      int x = int(mod(gl_FragCoord.x, 4.0));
      int y = int(mod(gl_FragCoord.y, 4.0));
      float threshold = bayerMatrix[x + y * 4];

      // Discard fragment if opacity is below the dither threshold
      if (opacity < threshold) {
          discard;
      }
  }
}

==FRAGMENT==