#include "texture_cache.hpp"

#include <vulkan/vulkan.h>

namespace gfx
{

void TextureCache::init(Device &device)
{
    m_device = &device;
}

void TextureCache::destroy()
{
    for (auto &pair : m_textures) {
        pair.second.first.destroy();
    }
}

void TextureCache::loadTexture(const fs::path &path, const std::string &name)
{
    fs::path texturePath = fs::path(TEXTURE_DIR_STR) / path;

    if (m_textures.find(name) != m_textures.end()) {
        return;
    }

    /// stb_image uploads RGBA8 in R,G,B,A order; must not use BGRA swapchain formats.
    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    Image image = m_device->loadImage(
        texturePath,
        format,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false
    );

    if (!image.isValid()) {
        throw std::runtime_error("Failed to load texture: " + path.string());
    }
    
    u32 textureID = m_device->addTexture(image);
    if (textureID == ~0u) {
        throw std::runtime_error("Failed to add texture to bindless manager.");
    }

    m_textures[name] = {image, textureID};
}

} // namespace gfx