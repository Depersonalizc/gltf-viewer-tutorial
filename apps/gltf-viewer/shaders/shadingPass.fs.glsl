#version 330

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;
uniform float uOcclusionStrength;

// GBuffers: Everything is in view space
uniform sampler2D uGPosition;
uniform sampler2D uGNormal;
uniform sampler2D uGDiffuse;
uniform sampler2D uGMetalRoughness;
uniform sampler2D uGEmissive;

// Screen Space Ambiant Occlusion
uniform sampler2D uSSAO;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;
const vec3 black = vec3(0);
const vec3 dielectricSpecular = vec3(0.04f);

// We need some simple tone mapping functions
// Basic gamma = 2.2 implementation
// Stolen here: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/shaders/tonemapping.glsl

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color)
{
  return pow(color, vec3(INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
  return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

void main()
{
  vec3 position = vec3(texelFetch(uGPosition, ivec2(gl_FragCoord.xy), 0));
  vec3 normal = vec3(texelFetch(uGNormal, ivec2(gl_FragCoord.xy), 0));
  
  vec3 N = normal;
  vec3 L = uLightDirection;
  vec3 V = normalize(-position);
  vec3 H = normalize(L + V);

  float NdotL = clamp(dot(N, L), 0, 1);
  float NdotH = clamp(dot(N, H), 0, 1);
  float VdotH = clamp(dot(V, H), 0, 1);
  float NdotV = clamp(dot(N, V), 0, 1);

  // Diffuse
  vec3 baseColor = vec3(texelFetch(uGDiffuse, ivec2(gl_FragCoord.xy), 0));

  // Metallic / Roughness
  float metallic = vec3(texelFetch(uGMetalRoughness, ivec2(gl_FragCoord.xy), 0)).b;
  float roughness = vec3(texelFetch(uGMetalRoughness, ivec2(gl_FragCoord.xy), 0)).g;

  // Occlusion map
  float occlusion = vec3(texelFetch(uGMetalRoughness, ivec2(gl_FragCoord.xy), 0)).r;
  if (N != 0.f && occlusion == 0.f) occlusion = 1.f;

  // Emissive
  vec3 emissive = vec3(texelFetch(uGEmissive, ivec2(gl_FragCoord.xy), 0));

  // SSAO
  // float ambientOcclusion = texture(ssao, TexCoords).r;
  float ambientOcclusion = vec3(texelFetch(uSSAO, ivec2(gl_FragCoord.xy), 0)).r;

  float alpha = roughness * roughness;
  float alpha_squared = alpha * alpha;

  // Microfacet Distribution (D)
  // Trowbridge-Reitz
  float D =                                               alpha_squared
            / /* -----------------------------------------------------------------------------------------------*/
                  (M_PI * ((NdotH*NdotH * (alpha_squared - 1) + 1) * (NdotH*NdotH * (alpha_squared - 1) + 1)));

  // Surface Reflection Ratio (F)
  // Fresnel Schlick
  vec3 F0 = mix(dielectricSpecular, baseColor, metallic);
  float baseShlickFactor = (1 - VdotH);
  float shlickFactor = baseShlickFactor * baseShlickFactor; // power 2
  shlickFactor *= shlickFactor; // power 4
  shlickFactor *= baseShlickFactor; // power 5
  vec3 F = F0 + (1 - F0) * shlickFactor;

  // Geometric Occlusion (G)
  // Smith Joint GGX
  float visDenominator = NdotL * sqrt(NdotV*NdotV * (1-alpha_squared) + alpha_squared) + NdotV * sqrt(NdotL*NdotL * (1-alpha_squared) + alpha_squared);
  float Vis;
  if (visDenominator > 0) {
    Vis = .5f / visDenominator;
  } else {
    Vis = 0.f;
  }

  vec3 c_diff = mix(baseColor * (1 - dielectricSpecular.r), black, metallic);
  vec3 diffuse = c_diff * M_1_PI;
  vec3 f_diffuse = (1.f - F) * diffuse;
  vec3 f_specular = F * Vis * D;

  vec3 nonOccludedColor = (f_diffuse + f_specular) * uLightIntensity * NdotL;
  vec3 occludedColor = mix(nonOccludedColor, nonOccludedColor * occlusion, uOcclusionStrength);
  occludedColor *= ambientOcclusion;

  fColor = LINEARtoSRGB(occludedColor + emissive);
}