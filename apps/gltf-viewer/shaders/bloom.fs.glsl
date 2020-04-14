#version 330

in vec2 vTexCoords;

uniform sampler2D uScene;
uniform sampler2D uBloomBlur;
uniform bool uUseBloom;
uniform vec3 uBloomTint;
uniform float uBloomIntensity;
uniform float uExposure;

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
    const float gamma = 2.2;
    vec3 hdrColor = SRGBtoLINEAR(texture(uScene, vTexCoords)).rgb;      
    vec3 bloomColor = SRGBtoLINEAR(texture(uBloomBlur, vTexCoords)).rgb;
    vec3 bloomTint = vec3(.2f, .7f, 1.f);
    bloomColor = bloomColor * bloomTint * uBloomIntensity;
    if (uUseBloom)
        hdrColor += bloomColor; // additive blending
    // tone mapping
    vec3 result = vec3(1.0) - exp(-hdrColor * uExposure);
    // also gamma correct while we're at it       
    // result = pow(result, vec3(1.0 / gamma));
    result = LINEARtoSRGB(result);
    fColor = bloomColor * uBloomIntensity;
    fColor = result;
}