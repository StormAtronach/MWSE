#include "NIRenderer.h"

#include "NIPixelData.h"

namespace NI {
	char* Renderer::getDriverInfo() {
		return vTable.asRenderer->getDriverInfo(this);
	}

	unsigned int Renderer::getCapabilityFlags() {
		return vTable.asRenderer->getCapabilityFlags(this);
	}

	const Texture::FormatPrefs* Renderer::findClosestPixelFormat(Texture::FormatPrefs* toFormat) {
		return vTable.asRenderer->findClosestPixelFormat(this, toFormat);
	}

	int Renderer::getBackBufferWidth() const {
		return vTable.asRenderer->getBackBufferWidth(this);
	}

	int Renderer::getBackBufferHeight() const {
		return vTable.asRenderer->getBackBufferHeight(this);
	}

	bool Renderer::setRenderTarget(RenderedTexture* texture) {
		return vTable.asRenderer->setRenderTarget(this, texture);
	}

	PixelData* Renderer::takeScreenshot(const Rect<unsigned int>* bounds) {
		auto pixelData = vTable.asRenderer->takeScreenshot(this, bounds);

#if defined(SE_IS_MWSE) && SE_IS_MWSE == 1
		if (pixelData == nullptr) {
			return nullptr;
		}
		const auto desiredFormat = pixelData->pixelFormat.getD3DFormat();

		// For some reason the game needs to swap B and R pixels.
		// See paper doll code, or around 0x42F939.
		if (desiredFormat == D3DFMT_X8R8G8B8) {
			const auto pixels = reinterpret_cast<PixelRGBA*>(pixelData->pixels);
			const auto pixelCount = pixelData->getWidth() * pixelData->getHeight();
			for (auto i = 0u; i < pixelCount; ++i) {
				auto& pixel = pixels[i];
				std::swap(pixel.r, pixel.b);
			}
		}
#endif
		return pixelData;
	}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
	PixelData* Renderer::takeScreenshot_lua(sol::optional<sol::table> bounds) {
		if (!bounds) {
			return takeScreenshot(nullptr);
		}

		const auto& values = bounds.value();
		if (values.size() != 4) {
			throw std::invalid_argument("Invalid 'bounds' parameter provided. It must have the four values left, right, top and bottom.");
		}
		const Rect<unsigned int> rect(values.get<unsigned int>(1), values.get<unsigned int>(2), values.get<unsigned int>(3), values.get<unsigned int>(4));
		if (rect.left > rect.right || rect.top > rect.bottom) {
			throw std::invalid_argument("Invalid 'bounds' parameter provided. Left must not be past right, and top must not be past bottom.");
		}
		return takeScreenshot(&rect);
	}
#endif

	bool Renderer::getTextureMemoryStats(unsigned int& total, unsigned int& available) {
		return vTable.asRenderer->getTextureMemoryStats(this, total, available);
	}

	bool Renderer::getTextureStats(unsigned int& loadedTextures, unsigned int& usedTextures, unsigned int& stateChanges, unsigned int& newTextures, unsigned int& evictedTextures, unsigned int& bytesTransferred) {
		return vTable.asRenderer->getTextureStats(this, loadedTextures, usedTextures, stateChanges, newTextures, evictedTextures, bytesTransferred);
	}
}
