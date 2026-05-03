#pragma once

#include <glm/ext.hpp>

#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"

namespace wld
{

class Sky
{

public:
    void init(gfx::Device &device);
    void destroy();

    void render(
        VkCommandBuffer cmd,
        f32 dayPhase01,
        f32 weather01,
        const glm::vec3 &sunDirectionWorld
    );

private:
    gfx::Pipeline m_pipeline;
};

} // namespace wld
