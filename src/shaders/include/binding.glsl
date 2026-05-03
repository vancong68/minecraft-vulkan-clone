#ifndef GLOBAL_GLSL
#define GLOBAL_GLSL

#extension GL_EXT_nonuniform_qualifier : require

#define CAMERA_UBO_IDX 0

/// Flat std140 block (no nested struct) matches C++ CameraUBO byte-for-byte on all drivers.
layout(binding = 0, std140) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 ortho;
    vec4 camPosWs;
} camUbo[];

layout(binding = 2) uniform sampler2D texArr[];

#endif // GLOBAL_GLSL