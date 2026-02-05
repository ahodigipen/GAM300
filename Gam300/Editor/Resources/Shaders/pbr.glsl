#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 biTangent;
layout (location = 5) in ivec4 joints;
layout (location = 6) in vec4 weights;

#define MAX_WEIGHTS 4
#define MAX_JOINTS 100

out Vertex {
    vec3 position;
    vec3 normal;
    mat3 TBN; //tangent, bitangent, normal
    vec2 uv;
} vertex;
layout (location = 1) out vec3 viewPos;
layout (location = 2) out vec4 fragPosLight;

// Standard per-object model matrix (used for non-instanced rendering)
uniform mat4 modelMat;
uniform mat4 frustumMat; // proj * view

// Per-object joint matrices (used for non-instanced animated rendering)
uniform mat4 jointsMat[MAX_JOINTS];
uniform bool hasJoints = false;
uniform mat4 u_lightSpace;

// Instancing support - SSBO containing per-instance model matrices (binding 3)
layout(std430, binding = 3) buffer InstanceBuffer {
    mat4 instanceMatrices[];
};

// Animated instancing support - SSBO containing per-instance joint matrices (binding 4)
// Layout: [instance0_joint0, instance0_joint1, ..., instance0_joint99, instance1_joint0, ...]
// Access: instanceJointMatrices[(gl_InstanceID + u_baseInstance) * MAX_JOINTS + jointIndex]
layout(std430, binding = 4) buffer JointBuffer {
    mat4 instanceJointMatrices[];
};

// Instancing mode: 0 = none, 1 = static, 2 = animated
uniform int u_instancingMode = 0;
uniform uint u_baseInstance = 0;        // Base instance for transform SSBO lookup
uniform uint u_jointBaseInstance = 0;   // Base instance for joint SSBO lookup (animated only)

out vec4 fragPosLightSpace;

void main() {
    mat4 transform = mat4(1.0);
    mat4 worldMatrix;

    if (u_instancingMode == 2) {
        // ANIMATED INSTANCING: world matrix from SSBO, joints from joint SSBO
        worldMatrix = instanceMatrices[gl_InstanceID + u_baseInstance];

        if (hasJoints) {
            transform = mat4(0.0);
            // Joint SSBO only contains animated instances, so use separate base
            uint jointBase = (gl_InstanceID + u_jointBaseInstance) * MAX_JOINTS;

            for (int i = 0; i < MAX_WEIGHTS && joints[i] > -1; i++) {
                transform += instanceJointMatrices[jointBase + uint(joints[i])] * weights[i];
            }
        }
    }
    else if (u_instancingMode == 1) {
        // STATIC INSTANCING: world matrix from SSBO, no joints
        worldMatrix = instanceMatrices[gl_InstanceID + u_baseInstance];
        // transform stays as identity (no skeletal animation)
    }
    else {
        // NON-INSTANCED (mode 0): use uniforms for both world matrix and joints
        worldMatrix = modelMat;

        if (hasJoints) {
            transform = mat4(0.0);
            for (int i = 0; i < MAX_WEIGHTS && joints[i] > -1; i++) {
                transform += jointsMat[joints[i]] * weights[i];
            }
        }
    }

    vertex.uv = vec2(uv.x, uv.y); //flip vertically due to opengl rendering logic
    transform = worldMatrix * transform;
    vertex.normal = mat3(transform) * normal;
    vertex.position = (transform * vec4(position, 1.0)).xyz;
    gl_Position = frustumMat * transform * vec4(position, 1.0);
    vertex.TBN = mat3(transform) * mat3(tangent, biTangent, normal);
    fragPosLightSpace = u_lightSpace * vec4(vertex.position, 1.0);
}
==VERTEX==

#version 450 core

in Vertex {
    vec3 position;
    vec3 normal;
    mat3 TBN;
    vec2 uv;
} vertex;

//shadow in vec4 pos
in vec4 fragPosLightSpace;

struct Material {
    vec3 emissive;
    vec3 albedo;
    float roughness;
    float metallic;
    float occlusion;
    float opacity;

    sampler2D occlusionMap;
    sampler2D roughnessMap;
    sampler2D emissiveMap;
    sampler2D metallicMap;
    sampler2D albedoMap;
    sampler2D normalMap;
    sampler2D opacityMap;

    bool isOcclusionMap;
    bool isRoughnessMap;
    bool isEmissiveMap;
    bool isMetallicMap;
    bool isAlbedoMap;
    bool isNormalMap;
    bool isOpacityMap;
};
uniform Material material;

// World-space UV mapping settings
uniform bool useWorldSpaceUV = false;
uniform float textureScale = 1.0;

const float PI = 3.14159265358979323846;
layout (location=0) out vec4 out_fragment;
layout(location=1) out vec4 out_brightness; //for bloom
const vec3 BLOOM_THRESHOLD = vec3(0.2126, 0.7152, 0.0722);
uniform float u_bloomThreshold = 1.0; // Brightness threshold for bloom extraction

//must also change in PBR.h limits
#define MAX_POINT_LIGHTS 512
#define MAX_DIR_LIGHTS   512
#define MAX_SPOT_LIGHTS  512

// GPU-friendly packed structs (std140-friendly: only vec4)
struct GPUPointLight {
    // xyz = position, w = range
    vec4 position_range;
    // rgb = radiance, w = intensity
    vec4 radiance_intensity;
};

struct GPUDirLight {
    // xyz = direction, w = intensity
    vec4 dir_intensity;
    // rgb = radiance, w unused
    vec4 radiance;
};

struct GPUSpotLight {
    // xyz = position, w = fallOff
    vec4 position_falloff;
    // xyz = direction, w = cutOff
    vec4 dir_cutoff;
    // rgb = radiance, w = intensity
    vec4 radiance_intensity;
};

// One UBO per type, with explicit bindings
layout(std140, binding = 0) uniform PointLightBlock {
    GPUPointLight pointLights[MAX_POINT_LIGHTS];
};

layout(std140, binding = 1) uniform DirLightBlock {
    GPUDirLight dirLights[MAX_DIR_LIGHTS];
};

layout(std140, binding = 2) uniform SpotLightBlock {
    GPUSpotLight spotLights[MAX_SPOT_LIGHTS];
};

// Light counts stay as normal uniforms
uniform int noPointLight;
uniform int noDirLight;
uniform int noSpotLight;
uniform vec3 viewPos;

uniform bool isDebugMode;
uniform bool showNormalTexture;

// shadow mapping - directional light
uniform sampler2D u_depthMap;
uniform bool u_enableShadows = false;

// shadow mapping - spot lights
#define MAX_SPOT_SHADOW_LIGHTS 4
uniform sampler2D u_spotDepthMaps[MAX_SPOT_SHADOW_LIGHTS];
uniform mat4 u_spotLightSpaceMatrices[MAX_SPOT_SHADOW_LIGHTS];
uniform int u_numSpotShadows = 0;

float ComputeShadow()
{
  // Early return if shadows are disabled
  if (!u_enableShadows) return 0.0;

  vec3 uvs = (fragPosLightSpace.xyz / fragPosLightSpace.w) * 0.5 + 0.5;

  // Check if fragment is outside light frustum
  if(uvs.x < 0.0 || uvs.x > 1.0 || uvs.y < 0.0 || uvs.y > 1.0 || uvs.z > 1.0)
    return 0.0; // not in shadow

  // Add bias to reduce shadow acne
  // Increase if you see shadow acne (noise)
  // Decrease if shadows float (peter panning)
  float bias = 0.005;

  // PCF (Percentage Closer Filtering) for softer shadows
  float shadow = 0.0;
  vec2 texelSize = 1.0 / textureSize(u_depthMap, 0);

  // 3x3 PCF kernel - samples 9 points around the fragment
  for(int x = -1; x <= 1; ++x)
  {
    for(int y = -1; y <= 1; ++y)
    {
      float pcfDepth = texture(u_depthMap, uvs.xy + vec2(x, y) * texelSize).r;
      shadow += uvs.z - bias > pcfDepth ? 1.0 : 0.0;
    }
  }

  // Average the 9 samples for soft shadow edges
  return shadow / 9.0;
}

// Compute shadow for a specific spot light
// Compute triplanar UVs based on world position and normal
// This projects the texture from 3 axes and blends based on normal direction
vec2 ComputeWorldSpaceUV(vec3 worldPos, vec3 worldNormal) {
    vec3 absNormal = abs(worldNormal);

    // Determine dominant axis for simpler planar projection
    // This gives a cleaner result than full triplanar blending for architectural surfaces
    vec2 uv;
    if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
        // Top/bottom face - project from Y axis (floor/ceiling)
        uv = worldPos.xz;
    } else if (absNormal.x > absNormal.z) {
        // Left/right face - project from X axis
        uv = worldPos.zy;
    } else {
        // Front/back face - project from Z axis
        uv = worldPos.xy;
    }

    // Prevent division by zero
    float scale = max(textureScale, 0.001);
    return uv / scale;
}

// Get the appropriate UV coordinates based on useWorldSpaceUV setting
vec2 GetTextureUV() {
    if (useWorldSpaceUV) {
        return ComputeWorldSpaceUV(vertex.position, vertex.normal);
    }
    return vertex.uv;
}

float ComputeSpotShadow(int lightIndex)
{
  // Check if this light has shadow mapping enabled
  if (lightIndex < 0 || lightIndex >= u_numSpotShadows) return 0.0;

  // Transform fragment position to light space
  vec4 fragPosSpotLight = u_spotLightSpaceMatrices[lightIndex] * vec4(vertex.position, 1.0);
  vec3 uvs = (fragPosSpotLight.xyz / fragPosSpotLight.w) * 0.5 + 0.5;

  // Check if fragment is outside light frustum
  if(uvs.x < 0.0 || uvs.x > 1.0 || uvs.y < 0.0 || uvs.y > 1.0 || uvs.z > 1.0)
    return 0.0; // not in shadow (outside light cone)

  float bias = 0.005;

  // PCF for softer shadows
  float shadow = 0.0;
  vec2 texelSize = 1.0 / textureSize(u_spotDepthMaps[lightIndex], 0);

  // 3x3 PCF kernel
  for(int x = -1; x <= 1; ++x)
  {
    for(int y = -1; y <= 1; ++y)
    {
      float pcfDepth = texture(u_spotDepthMaps[lightIndex], uvs.xy + vec2(x, y) * texelSize).r;
      shadow += uvs.z - bias > pcfDepth ? 1.0 : 0.0;
    }
  }

  return shadow / 9.0;
}

//this effect influences the appearance of surfaces
// for example, higher reflectivity at grazing angles than dielectrics
// f0 stands for the base fresnel distributivity
vec3 FresnelSchlick(float cosTheta, vec3 f0);
//approximate Fresnel factor for the microfacet distribution model
// for the material's reflectivity based on viewing angle
float GeometrySchlickGGX(float nDotV, float roughness);
//computes visibility term that accounts for self-shadowing 
// and masking effects due to microfacet distribution
float GeometrySmithGGX(float nDotV, float nDotL, float roughness);
//Trowbridge-Reitz calculates the probability distribution 
// of surface normals based on surface roughness
float DistributionGGX(vec3 N, vec3 H, float roughness);
vec3 ComputePointLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);
vec3 ComputeDirLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);
vec3 ComputeSpotLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);

vec3 ComputeMapOrMatV3(bool isMap, sampler2D map, vec3 mat, vec2 texUV) {
    vec3 res = mat;
    if (isMap) {
        res = texture(map, texUV).rgb;
    }
    return res;
}
float ComputeMapOrMatF(bool isMap, sampler2D map, float mat, vec2 texUV) {
    float res = mat;
    if (isMap) {
        res = texture(map, texUV).r;
    }
    return res;
}

const int bayer64[64] = int[64](0,  32,  8,  40, 2, 34, 10, 42,
                                48, 16, 56, 25, 50, 18, 58, 26,
                                12, 44, 4, 36, 14, 46, 6, 38,
                                60, 28, 52, 20, 62, 30, 54, 22,
                                3, 35, 11, 43, 1, 33, 8, 41,
                                51, 19, 59, 27, 49, 17, 57, 25,
                                15, 47, 7, 39, 13, 45, 5, 37,
                                63, 31, 55, 23, 61, 29, 53, 21
                                );

uniform float ditherThreshold;
uniform float ambientStrength;
void main() {
    if (isDebugMode) {
        //                                       green                strength
        out_fragment = vec4(mix(material.albedo, vec3(0.0, 1.0, 0.0), 0.75), 1.0);
        return;
    }
    if (showNormalTexture){
        out_fragment = vec4(normalize(vertex.normal) * 0.5 + 0.5, 1.0); //normal map colors
        return;
    }
    vec3 V = normalize(viewPos - vertex.position);

    // Get texture coordinates (world-space or mesh UV based on setting)
    vec2 texUV = GetTextureUV();

    //material or texture maps
    vec3 N = normalize(vertex.normal);
    if (material.isNormalMap) {
        N = 2.0 * texture(material.normalMap, texUV).rgb - 1.0;
        N = normalize(vertex.TBN * N);
    }
    vec3 albedo = ComputeMapOrMatV3(material.isAlbedoMap, material.albedoMap, material.albedo, texUV);
    float roughness = ComputeMapOrMatF(material.isRoughnessMap, material.roughnessMap, material.roughness, texUV);
    float metallic = ComputeMapOrMatF(material.isMetallicMap, material.metallicMap, material.metallic, texUV);
    vec3 emissive = ComputeMapOrMatV3(material.isEmissiveMap, material.emissiveMap, material.emissive, texUV);
    float occlusion = ComputeMapOrMatF(material.isOcclusionMap, material.occlusionMap, material.occlusion, texUV);
    float opacity = ComputeMapOrMatF(material.isOpacityMap, material.opacityMap, material.opacity, texUV);

    //fresnel reflectivity
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    vec3 ambient = ambientStrength * albedo;

    // Calculate shadow factor once
    float shadow = ComputeShadow();

    // Lights - shadows only affect directional lights
    vec3 pointLight = ComputePointLights(N, V, f0, albedo, roughness, metallic);
    vec3 dirLight = ComputeDirLights(N, V, f0, albedo, roughness, metallic);
    vec3 spotLight = ComputeSpotLights(N, V, f0, albedo, roughness, metallic);

    // Apply shadow only to directional light
    vec3 color = pointLight + (dirLight * (1.0 - shadow)) + spotLight;

    // Apply occlusion to all lighting
    color = color * occlusion;

    // Add ambient light (not affected by shadows, only by AO)
    color += ambient * occlusion;

    // Add emissive (not affected by lighting or shadows)
    color += emissive;

    // Extract bright areas for bloom with soft threshold to prevent flickering
    float brightness = dot(color, BLOOM_THRESHOLD);
    float soft = clamp((brightness - u_bloomThreshold) / max(u_bloomThreshold, 0.001), 0.0, 1.0);
    out_brightness = vec4(color * soft, 1.0);

    //simulate low bit depth
    float colorDepth = 32.0;
    vec3 quanColor = floor(color * colorDepth) / colorDepth; //5bits per channel, 32 levels

    //dither logic
    int col = int(mod(gl_FragCoord.x, 8));
    int row = int(mod(gl_FragCoord.y, 8));
    float threshold = float(bayer64[col + 8 * row]) / 64.0;
    if (length(color - quanColor) <= threshold * ditherThreshold) color = quanColor;

    out_fragment = vec4(color, opacity);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float GeometrySchlickGGX(float nDotV, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    float denom = nDotV * (1.0 - k) + k;
    return nDotV / denom;
}
float GeometrySmithGGX(float nDotV, float nDotL, float roughness) {
    float ggx1 = GeometrySchlickGGX(nDotV, roughness);
    float ggx2 = GeometrySchlickGGX(nDotL, roughness);
    return ggx1 * ggx2;
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float aSq = (roughness * roughness) * (roughness * roughness);
    float nDotH = max(dot(N, H), 0.0);
    float nDotHSq = nDotH * nDotH;
    float denom = nDotHSq * (aSq - 1.0) + 1.0;
    return aSq / (PI * denom * denom);
}
vec3 ComputePointLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0); 

    // clamp by both count & array size
    int count = min(noPointLight, MAX_POINT_LIGHTS);

    for (int i = 0; i < count; ++i) {
        GPUPointLight pl = pointLights[i];

        vec3  lightPos      = pl.position_range.xyz;
        float lightRange    = pl.position_range.w;
        vec3  lightRadiance = pl.radiance_intensity.rgb;
        float lightIntensity= pl.radiance_intensity.w;

        vec3 L = normalize(lightPos - vertex.position);
        vec3 H = normalize(L + V);
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);

        // Cook�Torrance BRDF (same as before)
        float NDF = DistributionGGX(N, H, roughness);
        vec3  FS  = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS  = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse  = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotL * nDotV, 0.0001);

        float dist = length(lightPos - vertex.position);
        if (dist > lightRange)
            continue;

        float attenuation = lightIntensity / (dist * dist);

        result += (diffuse + specular) * lightRadiance * attenuation * nDotL;
    }
    return result;
}

vec3 ComputeDirLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0);
    int count = min(noDirLight, MAX_DIR_LIGHTS);

    for (int i = 0; i < count; ++i) {
        GPUDirLight dl = dirLights[i];

        vec3  dir        = normalize(dl.dir_intensity.xyz);
        float intensity  = dl.dir_intensity.w;
        vec3  radiance   = dl.radiance.rgb;

        vec3 L = -dir;
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);
        vec3 H = normalize(L + V);

        float NDF = DistributionGGX(N, H, roughness);
        vec3  FS  = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS  = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse  = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotL * nDotV, 0.0001);

        result += (diffuse + specular) * radiance * intensity * nDotL;
    }
    return result;
}
vec3 ComputeSpotLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0);

    // make sure we don't run past the array
    int count = min(noSpotLight, MAX_SPOT_LIGHTS);

    for (int i = 0; i < count; ++i) {
        GPUSpotLight sl = spotLights[i];

        vec3  lightPos      = sl.position_falloff.xyz;
        float fallOff       = sl.position_falloff.w;
        vec3  lightDir      = normalize(sl.dir_cutoff.xyz);
        float cutOff        = sl.dir_cutoff.w;
        vec3  radiance      = sl.radiance_intensity.rgb;
        float intensity     = sl.radiance_intensity.w;

        vec3 L = normalize(lightPos - vertex.position);
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);
        vec3 H = normalize(L + V);

        // BRDF
        float NDF = DistributionGGX(N, H, roughness);
        vec3  FS  = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS  = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse  = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotV * nDotL, 0.0001);

        // spot shape
        float theta    = dot(L, normalize(-lightDir));
        float epsilon  = max(fallOff - cutOff, 0.0001);
        float spotFactor = clamp((theta - cutOff) / epsilon, 0.0, 1.0);

        float dist        = length(lightPos - vertex.position);
        float attenuation = intensity / (dist * dist);

        // Compute shadow for this spot light (only first MAX_SPOT_SHADOW_LIGHTS can cast shadows)
        float shadow = 0.0;
        if (i < MAX_SPOT_SHADOW_LIGHTS && u_enableShadows) {
            shadow = ComputeSpotShadow(i);
        }

        // Apply shadow to light contribution
        result += (diffuse + specular) *
                  radiance *
                  intensity *
                  attenuation *
                  nDotL *
                  spotFactor *
                  (1.0 - shadow);
    }

    return result;
}
==FRAGMENT==
