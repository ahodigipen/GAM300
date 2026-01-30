#version 450 core
layout (location = 0) in vec3 a_position;
layout (location = 5) in ivec4 joints;
layout (location = 6) in vec4 weights;

uniform mat4 u_lightSpace;
uniform mat4 u_model;

#define MAX_WEIGHTS 4
#define MAX_JOINTS 100

uniform mat4 jointsMat[MAX_JOINTS];
uniform bool hasJoints = false;

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
  gl_Position = u_lightSpace * u_model * transform * vec4(a_position, 1.0f);
}

==VERTEX==

#version 450 core

void main() 
{ 
  // gl_Depth = ...
}

==FRAGMENT==