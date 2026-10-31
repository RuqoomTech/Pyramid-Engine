#version 330 core

in vec2 v_TexCoord;

// G-Buffer textures
uniform sampler2D u_GAlbedoMetallic;
uniform sampler2D u_GNormalRoughness;
uniform sampler2D u_GPositionAO;
uniform sampler2D u_GEmissive;
uniform sampler2D u_GDepth;

// Separate owned 2D depth targets; keep the limit in sync with MaxShadowCascades.
uniform sampler2D u_ShadowMaps[8];
uniform mat4 u_LightSpaceMatrices[8];
uniform float u_CascadeSplits[9];
uniform int u_CascadeCount;

// This immediate pass receives camera data independently of queued uniform blocks.
uniform mat4 u_View;
uniform vec3 u_CameraPosition;

// Lighting
uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform float u_ShadowBias;

uniform bool u_EnableSSAO;
uniform bool u_EnableIBL;

out vec4 FragColor;

const float PI = 3.14159265359;

// PBR functions
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// GLSL 3.30 requires constant integral sampler-array indices. Explicit LOD also
// avoids implicit derivatives inside fragment-dependent cascade branches.
float SampleShadowDepth(int cascadeIndex, vec2 uv) {
    switch (cascadeIndex) {
        case 0: return textureLod(u_ShadowMaps[0], uv, 0.0).r;
        case 1: return textureLod(u_ShadowMaps[1], uv, 0.0).r;
        case 2: return textureLod(u_ShadowMaps[2], uv, 0.0).r;
        case 3: return textureLod(u_ShadowMaps[3], uv, 0.0).r;
        case 4: return textureLod(u_ShadowMaps[4], uv, 0.0).r;
        case 5: return textureLod(u_ShadowMaps[5], uv, 0.0).r;
        case 6: return textureLod(u_ShadowMaps[6], uv, 0.0).r;
        case 7: return textureLod(u_ShadowMaps[7], uv, 0.0).r;
    }
    return 1.0;
}

vec2 ShadowTexelSize(int cascadeIndex) {
    switch (cascadeIndex) {
        case 0: return 1.0 / vec2(textureSize(u_ShadowMaps[0], 0));
        case 1: return 1.0 / vec2(textureSize(u_ShadowMaps[1], 0));
        case 2: return 1.0 / vec2(textureSize(u_ShadowMaps[2], 0));
        case 3: return 1.0 / vec2(textureSize(u_ShadowMaps[3], 0));
        case 4: return 1.0 / vec2(textureSize(u_ShadowMaps[4], 0));
        case 5: return 1.0 / vec2(textureSize(u_ShadowMaps[5], 0));
        case 6: return 1.0 / vec2(textureSize(u_ShadowMaps[6], 0));
        case 7: return 1.0 / vec2(textureSize(u_ShadowMaps[7], 0));
    }
    return vec2(0.0);
}

float CalculateShadow(vec3 worldPos, vec3 normal) {
    // Never consult shadow textures or metadata for a disabled/incomplete frame.
    if (u_CascadeCount <= 0 || u_CascadeCount > 8) return 0.0;

    float depth = -(u_View * vec4(worldPos, 1.0)).z;
    if (depth < u_CascadeSplits[0] || depth >= u_CascadeSplits[u_CascadeCount]) return 0.0;
    int cascadeIndex = u_CascadeCount - 1;
    for (int i = 0; i < u_CascadeCount; i++) {
        if (depth < u_CascadeSplits[i + 1]) {
            cascadeIndex = i;
            break;
        }
    }

    vec4 lightSpacePos = u_LightSpaceMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    if (lightSpacePos.w <= 0.00001) return 0.0;
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0)))) return 0.0;

    float bias = max(u_ShadowBias * (1.0 - dot(normal, -u_LightDirection)), u_ShadowBias * 0.1);
    float shadow = 0.0;
    vec2 texelSize = ShadowTexelSize(cascadeIndex);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = SampleShadowDepth(cascadeIndex, projCoords.xy + vec2(x, y) * texelSize);
            shadow += (projCoords.z - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    // Sample G-Buffer
    vec4 albedoMetallic = texture(u_GAlbedoMetallic, v_TexCoord);
    vec4 normalRoughness = texture(u_GNormalRoughness, v_TexCoord);
    vec4 positionAO = texture(u_GPositionAO, v_TexCoord);
    vec4 emissive = texture(u_GEmissive, v_TexCoord);
    
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 normal = normalize(normalRoughness.rgb * 2.0 - 1.0); // Decode normal
    float roughness = normalRoughness.a;
    vec3 worldPos = positionAO.rgb;
    float ao = positionAO.a;
    
    // Calculate PBR lighting
    vec3 V = normalize(u_CameraPosition - worldPos);
    vec3 L = normalize(-u_LightDirection);
    vec3 H = normalize(V + L);
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(normal, L), 0.0);
    
    // Calculate shadow
    float shadow = CalculateShadow(worldPos, normal);
    
    // Direct lighting
    vec3 radiance = u_LightColor * u_LightIntensity;
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    
    // Ambient (simplified - should use IBL)
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    // Add emissive
    vec3 color = ambient + Lo + emissive.rgb;
    
    FragColor = vec4(color, 1.0);
}