#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;

// The geometry pass supplies this per draw through the command buffer.
uniform mat4 u_ViewProjection;

// Per-object uniforms
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;

out VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
} vs_out;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    vs_out.WorldPos = worldPos.xyz;
    
    vec3 T = normalize(vec3(u_Model * vec4(a_Tangent, 0.0)));
    vec3 N = normalize(vec3(u_NormalMatrix * vec4(a_Normal, 0.0)));
    vec3 B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);
    
    vs_out.Normal = N;
    vs_out.TexCoord = a_TexCoord;
    
    gl_Position = u_ViewProjection * worldPos;
}