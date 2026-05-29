#include "texture_cache.hpp"

#include <vulkan/vulkan.h>
#include <cstring>

namespace gfx
{

void TextureCache::init(Device &device)
{
    m_device = &device;
    createFallbackTexture();
}

void TextureCache::createFallbackTexture()
{
    constexpr u32 checkerSize = 8;
    constexpr u32 squareSize = 4;
    
    std::vector<u32> pixelData(checkerSize * checkerSize);
    u32 magenta = 0xFF00FFFF;
    u32 black = 0xFF000000;
    
    for (u32 y = 0; y < checkerSize; ++y) {
        for (u32 x = 0; x < checkerSize; ++x) {
            bool isEvenX = (x / squareSize) % 2 == 0;
            bool isEvenY = (y / squareSize) % 2 == 0;
            pixelData[y * checkerSize + x] = (isEvenX == isEvenY) ? magenta : black;
        }
    }
    
    m_fallbackImage = m_device->createImageFromData(
        pixelData.data(),
        checkerSize,
        checkerSize,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    
    if (m_fallbackImage.isValid()) {
        m_fallbackTextureID = m_device->addTexture(m_fallbackImage);
    }
    
    if (m_fallbackTextureID == ~0u) {
        throw std::runtime_error("Failed to create fallback texture.");
    }
}

void TextureCache::destroy()
{
    for (auto &pair : m_textures) {
        pair.second.first.destroy();
    }
    m_fallbackImage.destroy();
}

void TextureCache::loadTexture(const fs::path &path, const std::string &name)
{
    fs::path texturePath = fs::path(TEXTURE_DIR_STR) / path;

    if (m_textures.find(name) != m_textures.end()) {
        return;
    }

    // Try to locate the file in a few places to avoid working-directory issues.
    if (!fs::exists(texturePath)) {
        if (fs::exists(path)) {
            texturePath = path;
        } else {
            fs::path alt = fs::current_path() / path;
            if (fs::exists(alt)) {
                texturePath = alt;
            } else {
                std::cerr << "Warning: Texture file not found: " << texturePath.string()
                          << " (tried '" << path.string() << "' and '" << alt.string() << "')" << std::endl;
                m_textures[name] = {m_fallbackImage, m_fallbackTextureID};
                return;
            }
        }
    }

    /// stb_image uploads RGBA8 in R,G,B,A order; must not use BGRA swapchain formats.
    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    try {
        Image image = m_device->loadImage(
            texturePath,
            format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            false
        );

        if (!image.isValid()) {
            std::cerr << "Warning: Failed to load texture: " << texturePath.string() << " (invalid image)" << std::endl;
            m_textures[name] = {m_fallbackImage, m_fallbackTextureID};
            return;
        }
        
        u32 textureID = m_device->addTexture(image);
        if (textureID == ~0u) {
            std::cerr << "Warning: Failed to add texture to bindless manager: " << texturePath.string() << std::endl;
            image.destroy();
            m_textures[name] = {m_fallbackImage, m_fallbackTextureID};
            return;
        }

        m_textures[name] = {image, textureID};
    } catch (const std::exception &e) {
        std::cerr << "Warning: Exception loading texture " << texturePath.string() << ": " << e.what() << std::endl;
        m_textures[name] = {m_fallbackImage, m_fallbackTextureID};
    }
}

} // namespace gfx