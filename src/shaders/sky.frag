#version 450

layout(location = 0) in vec3 vWorldDir;
layout(location = 1) in vec4 skyParams;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform SkyPC {
    vec4 params;
    vec4 sunDirWorld;
} skyPc;

const float PI = 3.14159265;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    float dayPhase = skyParams.x;
    float weather = skyParams.y;
    vec3 dir = normalize(vWorldDir);
    vec3 sunDir = normalize(skyPc.sunDirWorld.xyz);
    vec3 moonDir = -sunDir;

    float day01 = sin(dayPhase * 6.2831853) * 0.5 + 0.5;
    float twilight = pow(1.0 - abs(day01 * 2.0 - 1.0), 3.0);

    vec3 zenithDay = vec3(0.38, 0.58, 1.0);
    vec3 zenithNight = vec3(0.015, 0.025, 0.08);
    vec3 horizonDay = vec3(0.78, 0.86, 1.0);
    vec3 horizonNight = vec3(0.06, 0.07, 0.14);

    vec3 zenith = mix(zenithNight, zenithDay, day01);
    vec3 horizon = mix(horizonNight, horizonDay, day01);

    float h = dir.y;
    float grad = smoothstep(-0.15, 0.45, h);
    vec3 skyColor = mix(horizon, zenith, grad);

    vec3 sunsetCol = vec3(1.0, 0.45, 0.25);
    float sunHeight = sunDir.y;
    skyColor = mix(skyColor, mix(skyColor, sunsetCol, 0.65),
        twilight * smoothstep(-0.05, 0.2, -sunHeight));

    float sunCos = max(dot(dir, sunDir), 0.0);
    float sunDisk = smoothstep(0.9992, 0.9999, sunCos);
    float sunGlow = pow(sunCos, 256.0) * 3.0 + pow(sunCos, 64.0) * 0.35;
    vec3 sunCol = vec3(1.0, 0.96, 0.88);
    float sunVis = smoothstep(-0.04, 0.02, sunDir.y);
    skyColor += (sunDisk * 1.4 + sunGlow) * sunCol * sunVis;

    float moonCos = max(dot(dir, moonDir), 0.0);
    float moonDisk = smoothstep(0.9986, 0.9995, moonCos);
    float moonGlow = pow(moonCos, 128.0) * 0.5;
    float moonVis = smoothstep(0.08, -0.02, sunDir.y);
    vec3 moonCol = vec3(0.92, 0.95, 1.0);
    skyColor += (moonDisk * 0.85 + moonGlow) * moonCol * moonVis;

    if (sunDir.y < 0.12) {
        float starMask = smoothstep(0.08, -0.07, sunDir.y) * smoothstep(-0.15, 0.28, h);
        float cell = hash(floor(dir * 280.0));
        float star = step(0.94, cell) * starMask * 0.35;
        skyColor += vec3(star);
    }

    vec3 weatherTint = vec3(0.62, 0.66, 0.72);
    skyColor = mix(skyColor, weatherTint * skyColor, weather);

    skyColor = pow(skyColor, vec3(0.92));
    outColor = vec4(skyColor, 1.0);
}
