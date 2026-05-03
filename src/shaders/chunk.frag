#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in flat uint lightLevel;
layout(location = 3) in flat uint faceDirection;

layout(push_constant) uniform PushConstantObject {
    layout(offset = 0) mat4 model;
    layout(offset = 64) mat4 shadowMatrix;
    layout(offset = 128) vec4 sunDir;
    layout(offset = 144) vec4 camWorldPos;
    layout(offset = 160) uint textureId;
    layout(offset = 164) uint shadowMapId;
} pco;

const uint SHADOW_OFF = 4294967295u;

float shadowDepthCompare(vec2 uv, float zRef, float bias)
{
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return 1.0;
    }
    float d = texture(texArr[pco.shadowMapId], uv).r;
    return (zRef - bias > d) ? 0.4 : 1.0;
}

float sampleShadow(vec3 wp)
{
    if (pco.shadowMapId == SHADOW_OFF) {
        return 1.0;
    }

    vec4 ls = pco.shadowMatrix * vec4(wp, 1.0);
    vec3 projCoords = ls.xyz / ls.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z = clamp(projCoords.z, 0.0, 1.0);

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float bias = 0.0022;
    vec2 texel = vec2(1.0 / 2048.0);
    float s = 0.0;
    s += shadowDepthCompare(projCoords.xy + vec2(0.0, 0.0), projCoords.z, bias);
    s += shadowDepthCompare(projCoords.xy + vec2(texel.x, 0.0), projCoords.z, bias);
    s += shadowDepthCompare(projCoords.xy + vec2(0.0, texel.y), projCoords.z, bias);
    s += shadowDepthCompare(projCoords.xy + texel, projCoords.z, bias);
    return s * 0.25;
}

vec3 addShadow(vec3 color)
{
    float faceLights[6] = float[](
        0.55, 0.55, 0.8, 0.8, 1.0, 0.35
    );

    vec3 faceNormals[6] = vec3[](
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 0.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0)
    );

    vec3 sun = normalize(pco.sunDir.xyz);
    float ndotl = max(dot(faceNormals[faceDirection], -sun), 0.0);
    float brightness = pow(lightLevel / 15.0, 2.2);
    float light = brightness * faceLights[faceDirection];
    float sunShade = mix(0.4, 1.0, ndotl);

    float sh = sampleShadow(worldPos);
    sunShade *= sh;

    float ambientLight = 0.1;
    vec3 litColor = mix(color * ambientLight, color, light * sunShade);
    litColor = mix(litColor, litColor * vec3(1.02, 1.0, 0.98), 0.15);

    float heightShadow = clamp((worldPos.y - 12.0) / 24.0, 0.0, 1.0);
    litColor *= mix(0.75, 1.0, heightShadow);

    return litColor;
}

vec3 addFog(vec3 color, float dist)
{
    /// Strong only past many blocks (horizon fade). Near camera stays nearly clear.
    float fogStart = 22.0 * 16.0;
    float fogEnd = 36.0 * 16.0;
    vec3 fogColor = vec3(0.72, 0.82, 0.98);

    float amt = smoothstep(fogStart, fogEnd, dist);
    amt = pow(clamp(amt, 0.0, 1.0), 1.15);

    return mix(color, fogColor, amt * 0.92);
}

void main()
{
    float dist = length(worldPos - pco.camWorldPos.xyz);

    vec4 albedo = texture(texArr[pco.textureId], fragUV);
    vec3 color = albedo.rgb;
    float alpha = albedo.a;

    if (alpha > 0.1 && alpha < 0.9) {
        alpha = 0.8;
    }

    if (alpha < 0.1) {
        discard;
    }

    color = addShadow(color);

    color = addFog(color, dist);

    outColor = vec4(color, alpha);
}
