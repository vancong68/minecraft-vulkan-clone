#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint depthTextureId;
    uint _pad0;
    uint _pad1;
    uint _pad2;
} pco;

void main()
{
    float depth = texture(texArr[pco.depthTextureId], fragUV).r;
    float m = (depth >= 0.9999) ? 1.0 : 0.0;
    outColor = vec4(m, m, m, 1.0);
}

