#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;

out vec3 fColor;

vec3 getRadiance() {
  float PI = 3.14;
  vec3 Li = uLightIntensity;
  return vec3(1./PI) * Li * dot(normalize(vViewSpaceNormal), uLightDirection);
}

void main()
{
  vec3 viewDirection = -vViewSpacePosition;
  vec3 radiance = getRadiance();
  fColor = radiance;
}
