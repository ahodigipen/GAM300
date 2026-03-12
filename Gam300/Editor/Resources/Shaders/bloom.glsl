#version 450 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 in_uv;
out vec2 uvs;

void main() 
{
	 gl_Position = vec4(pos, 0.0, 1.0);
	uvs = in_uv;
}

==VERTEX==

#version 450 core
out vec4 out_fragment;
in vec2 uvs;

const float WEIGHTS[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

uniform sampler2D u_brightnessMap;
uniform bool u_horizontalPass;
uniform int u_frameHeight;
uniform int u_frameWidth;

vec3 ClampSafe(vec3 v)
{
    v.x = (isnan(v.x) || isinf(v.x)) ? 0.0 : v.x;
    v.y = (isnan(v.y) || isinf(v.y)) ? 0.0 : v.y;
    v.z = (isnan(v.z) || isinf(v.z)) ? 0.0 : v.z;
    return v;
}

void main() 
{             
    vec3 color = ClampSafe(texture(u_brightnessMap, uvs).rgb) * WEIGHTS[0];

    // vec2 texelSize = 1.0 / textureSize(u_brightnessMap, 0); with opengl 4.x

    vec2 texelSize = 1.0 / vec2(u_frameWidth, u_frameHeight);

    if(u_horizontalPass) 
    {
        for(int i = 1; i < 5; ++i)
        {
            color += ClampSafe(texture(u_brightnessMap, uvs + vec2(texelSize.x * i, 0.0)).rgb * WEIGHTS[i]);
            color += ClampSafe(texture(u_brightnessMap, uvs - vec2(texelSize.x * i, 0.0)).rgb * WEIGHTS[i]);
        }
    }
    else 
    {
        for(int i = 1; i < 5; ++i) 
        {
            color += ClampSafe(texture(u_brightnessMap, uvs + vec2(0.0, texelSize.y * i)).rgb * WEIGHTS[i]);
            color += ClampSafe(texture(u_brightnessMap, uvs - vec2(0.0, texelSize.y * i)).rgb * WEIGHTS[i]);
        }
    }  
    out_fragment = vec4(color, 1.0);      
}

==FRAGMENT==