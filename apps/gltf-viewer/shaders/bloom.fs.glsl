#version 330

in vec2 vTexCoords;

uniform sampler2D uScene;
uniform sampler2D uBloomBlur;
uniform bool uUseBloom;
uniform vec3 uBloomTint;
uniform float uBloomIntensity;
uniform float uExposure;
uniform bool uShowBloomOnly;

const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;

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

out vec3 fColor;

void main()
{
  vec3 hdrColor = SRGBtoLINEAR(texture(uScene, vTexCoords)).rgb;      
  vec3 bloomColor = SRGBtoLINEAR(texture(uBloomBlur, vTexCoords)).rgb;
  bloomColor *= uBloomTint * uBloomIntensity;
  if (uUseBloom) {
    if (uShowBloomOnly) {
      hdrColor = bloomColor;
    } else {
      hdrColor += bloomColor; // Additive blending
    }
  }
  vec3 result = vec3(1.0) - exp(-hdrColor * uExposure); // Tone mapping
  fColor = LINEARtoSRGB(result); // Gamma Correction
}