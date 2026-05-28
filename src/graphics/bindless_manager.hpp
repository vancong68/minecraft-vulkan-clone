#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>

#include "core/types.hpp"

#include "buffer.hpp"

namespace gfx
{

class Device;  
class Image;

class BindlessManager
{

public:
    enum class ResourceType
    {
        UBO,
        SSBO,
        TEXTURE
    };

    BindlessManager() = default;
    ~BindlessManager() = default;

    void init(Device &device);
    void destroy();

    u32 addUBO(
        const Buffer &buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = 0
    );

    u32 addSSBO(
        const Buffer &buffer,
        VkDeviceSize offset = 0,
        VkDeviceSize range = 0
    );

    u32 addTexture(
        const Image &image,
        VkSampler sampler = VK_NULL_HANDLE
    );

    void removeResource(u32 id);

    void updateTexture(u32 id, const Image &image, VkSampler sampler = VK_NULL_HANDLE);

    void update();

public:
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

private:
    Device *m_device = nullptr;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    struct ResourceSlot
    {
        ResourceType type;
        u32 binding;
        u32 arrayIndex;

        union {
            struct {
                VkBuffer buffer;
                VkDeviceSize offset;
                VkDeviceSize range;
            };
            struct {
                VkImageView imageView;
                VkSampler sampler;
            };
        };

        bool isDirty = false;
        bool isUsed = false;
    };

    std::vector<ResourceSlot> m_resources;
    std::vector<u32> m_dirtyResources;
    std::mutex m_mutex;

    u32 m_nextUboIndex = 0;
    u32 m_nextSsboIndex = 0;
    u32 m_nextTextureIndex = 0;

    static constexpr u32 MAX_UBOS = 64;
    static constexpr u32 MAX_SSBOS = 64;
    static constexpr u32 MAX_TEXTURES = 256;

    static constexpr u32 UBO_BINDING = 0;
    static constexpr u32 SSBO_BINDING = 1;
    static constexpr u32 TEXTURE_BINDING = 2;

    u32 addResourceInternal(
        ResourceType type,
        u32 binding,
        u32 &localIndex,
        u32 maxCount
    );
    u32 getResourceBaseIndex(ResourceType type) const;

    static constexpr u32 HANDLE_TYPE_MASK = 0xC0000000u;
    static constexpr u32 HANDLE_INDEX_MASK = 0x3FFFFFFFu;
    static u32 makeHandle(ResourceType type, u32 index);
    static ResourceType getTypeFromHandle(u32 handle);

public:
    static u32 getLocalIndexFromHandle(u32 handle);

private:
    u32 getGlobalIndex(ResourceType type, u32 localIndex) const;

};

} // namespace gfx
