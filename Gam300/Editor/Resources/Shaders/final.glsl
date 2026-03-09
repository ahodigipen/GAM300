
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

uniform float u_gamma;      // sRGB gamma        (default 2.2)
uniform float u_exposure;   // HDR exposure scale (default 1.0)
uniform vec3  u_warmTint;   // color temperature  (default ~5000K warm)

uniform sampler2D map;
uniform sampler2D u_bloom;
uniform sampler2D u_depth;
uniform bool u_enableBloom;
uniform float u_fadeAlpha; // NEW: Fade to black alpha
uniform float u_bloomIntensity = 1.0; // Bloom intensity multiplier

// Volumetric Fog
uniform bool u_enableFog;
uniform vec3 u_fogColor;
uniform float u_fogDensity;
uniform float u_fogHeightFalloff;
uniform float u_fogHeight;
uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;

// Narkowicz 2015 ACES approximation — same curve used by Unity URP's ACES mode
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
  vec3 result = texture(map, uvs).rgb;

  if (u_enableBloom) {
      result += texture(u_bloom, uvs).rgb * u_bloomIntensity;
  }

  // Volumetric height fog (applied in linear space before tone mapping)
  if (u_enableFog) {
      float depthSample = texture(u_depth, uvs).r;
      vec3 ndc = vec3(uvs * 2.0 - 1.0, depthSample * 2.0 - 1.0);
      vec4 worldPos4 = u_invViewProj * vec4(ndc, 1.0);
      worldPos4 /= worldPos4.w;
      float dist = length(worldPos4.xyz - u_cameraPos);
      float heightFog = exp(-max(0.0, worldPos4.y - u_fogHeight) * max(u_fogHeightFalloff, 0.0001));
      float fogAmount = 1.0 - exp(-u_fogDensity * dist * heightFog);
      fogAmount = clamp(fogAmount, 0.0, 1.0);
      result = mix(result, u_fogColor, fogAmount);
  }

  result *= u_exposure;
  result *= u_warmTint;
  result = ACESFilm(result);
  result = pow(result, vec3(1.0 / u_gamma));

  // Apply fade to black
  result = mix(result, vec3(0.0), u_fadeAlpha);

  out_fragment = vec4(result, 1.0);
}

==FRAGMENT==