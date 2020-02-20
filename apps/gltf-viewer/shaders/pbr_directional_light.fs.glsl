#version 330

in vec3 vViewSpaceNormal;
in vec3 vViewSpacePosition;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;
uniform vec4 uBaseColorFactor;
uniform sampler2D uBaseColorTexture;
uniform float uMetallicFactor;
uniform float uRougnessFactor;
uniform sampler2D uMetallicRoughnessTexture;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;
const vec3 black = vec3(0, 0, 0);
const vec3 dielectricSpecular = vec3(0.04f, 0.04f, 0.04f);

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
  vec3 N = normalize(vViewSpaceNormal);
  vec3 L = uLightDirection;
  vec3 V = normalize(-vViewSpacePosition);
  vec3 H = normalize(L + V);

  float NdotL = clamp(dot(N, L), 0, 1);
  float NdotH = clamp(dot(N, H), 0, 1);
  float VdotH = clamp(dot(V, H), 0, 1);
  float NdotV = clamp(dot(N, V), 0, 1);

  vec4 baseColorFromTexture = SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
  vec4 baseColor = baseColorFromTexture * uBaseColorFactor;
  
  float metallic = texture(uMetallicRoughnessTexture, vTexCoords).b * uMetallicFactor;
  float roughness = texture(uMetallicRoughnessTexture, vTexCoords).g * uRougnessFactor;

  float alpha = roughness * roughness;
  float alpha_squared = alpha * alpha;

  vec3 F0 = mix(dielectricSpecular, baseColor.rgb, metallic);

  float D =                                               alpha_squared
            / /* -----------------------------------------------------------------------------------------------*/
                  (M_PI * ((NdotH*NdotH * (alpha_squared - 1) + 1) * (NdotH*NdotH * (alpha_squared - 1) + 1)));

  //  Fresnel Schlick
  float baseShlickFactor = (1 - VdotH);
  float shlickFactor = baseShlickFactor * baseShlickFactor; // power 2
  shlickFactor *= shlickFactor; // power 4
  shlickFactor *= baseShlickFactor; // power 5
  vec3 F = F0 + (1 - F0) * shlickFactor;

  float visDenominator = (NdotL * sqrt(NdotV*NdotV * (1-alpha_squared) + alpha_squared) + NdotV * sqrt(NdotL*NdotL * (1-alpha_squared) + alpha_squared));
  float Vis;
  if (visDenominator > 0) {
    Vis = .5f / visDenominator;
  } else {
    Vis = 0.f;
  }
  
  vec3 c_diff = mix(baseColor.rgb * (1 - dielectricSpecular.r), black, metallic);
  vec3 diffuse = c_diff * M_1_PI;

  vec3 f_diffuse = (1 - F) * diffuse;
  vec3 f_specular = F * Vis * D;

  fColor = LINEARtoSRGB((f_diffuse + f_specular) * uLightIntensity * NdotL);

  // diffuse = f_diffuse * NdotL;
  // fColor = LINEARtoSRGB(diffuse);
}