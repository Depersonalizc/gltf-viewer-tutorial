#version 330

in vec2 vTexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D uNoiseTex;

uniform vec3 samples[64];
uniform mat4 uProjection;

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
uniform int uKernelSize; // maximum kernel size = 64
uniform float uRadius;
uniform float uBias;
uniform float uIntensity;

// tile noise texture over screen based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(1280.0/4.0, 720.0/4.0); 

out float fColor;

void main()
{
    // Get input for SSAO algorithm
    vec3 fragPos = vec3(texelFetch(gPosition, ivec2(gl_FragCoord.xy), 0));
    vec3 normal = vec3(texelFetch(gNormal, ivec2(gl_FragCoord.xy), 0));
    vec3 randomVec = normalize(texture(uNoiseTex, vTexCoords * noiseScale).xyz);

    // Create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for (int i = 0; i < uKernelSize; ++i)
    {
        // Get sample position
        vec3 sample = TBN * samples[i]; // From tangent to view-space
        sample = fragPos + sample * uRadius; 
        
        // Project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(sample, 1.0);
        offset = uProjection * offset; // From view to clip-space
        offset.xyz /= offset.w; // Perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // Transform to range 0.0 - 1.0
        
        // Get sample depth
        float sampleDepth = texture(gPosition, offset.xy).z; // Get depth value of kernel sample
        
        // Range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= sample.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / uKernelSize);
    
    fColor = pow(occlusion, uIntensity);
}