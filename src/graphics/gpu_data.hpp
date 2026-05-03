#pragma once

#include "core/types.hpp"
#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "core/camera/camera.hpp"

namespace gfx
{

constexpr u32 CAMERA_UBO = 0;
constexpr u32 TIME_UBO = 1;
constexpr u32 LIGHT_UBO = 2;

/// Layout must match binding.glsl (std140-style): vec4 avoids vec3 packing mismatch.
struct CameraUBO
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 ortho;
    alignas(16) glm::vec4 position;
};

struct TimeUBO
{
    alignas(4) f32 time;
    alignas(4) f32 deltaTime;
};

struct LightUBO
{
    alignas(16) glm::mat4 lightSpace;
    alignas(16) glm::vec4 sunDir;
};

class GPUData
{

public:
    void init(Device &device);
    void destroy();

    void updateCamera(const core::Camera &camera);
    void updateTime(f32 time, f32 deltaTime);
    void updateLight(const glm::mat4 &lightSpace, const glm::vec3 &sunDir);

    void update();

private:
    Device *m_device = nullptr;

    Buffer m_cameraBuffer;
    Buffer m_timeBuffer;
    Buffer m_lightBuffer;

    void createBuffers();
};

}
