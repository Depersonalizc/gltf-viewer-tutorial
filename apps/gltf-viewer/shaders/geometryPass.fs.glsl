#version 330

in vec3 vViewSpaceNormal;
in vec3 vViewSpacePosition;
in vec2 vTexCoords;

uniform vec4 uBaseColorFactor;
uniform sampler2D uBaseColorTexture;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform sampler2D uMetallicRoughnessTexture;
uniform sampler2D uEmissiveTexture;
uniform vec3 uEmissiveFactor;
uniform sampler2D uOcclusionTexture;

layout(location = 0) out vec3 fPosition;
layout(location = 1) out vec3 fNormal;
layout(location = 2) out vec3 fDiffuse;
layout(location = 3) out vec3 fMetalRoughness;
layout(location = 4) out vec3 fEmissive;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;
const vec3 black = vec3(0, 0, 0);
const vec3 dielectricSpecular = vec3(0.04f, 0.04f, 0.04f);

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
  return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

void main()
{

  // Normal
  vec3 N = normalize(vViewSpaceNormal);

  // Diffuse
  vec4 baseColorFromTexture = SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
  vec4 baseColor = baseColorFromTexture * uBaseColorFactor;

  // Metallic / Roughness
  float metallic = texture(uMetallicRoughnessTexture, vTexCoords).b * uMetallicFactor;
  float roughness = texture(uMetallicRoughnessTexture, vTexCoords).g * uRoughnessFactor;

  // Emissive
  vec3 emissive = SRGBtoLINEAR(texture(uEmissiveTexture, vTexCoords)).rgb * uEmissiveFactor;

  // Occlusion
  float occlusion = texture(uOcclusionTexture, vTexCoords).r;

  // Deferred shading
  fPosition = vViewSpacePosition;
  fNormal = N;
  fDiffuse = baseColor.rgb;
  fMetalRoughness = vec3(occlusion, roughness, metallic);
  fEmissive = emissive;
}