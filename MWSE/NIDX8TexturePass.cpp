#include "NIDX8TexturePass.h"

namespace NI {
	const auto NI_DX8TexturePass_setCTPipelineState = reinterpret_cast<unsigned int(__thiscall*)(DX8TexturePass*, PropertyState*, int, int, unsigned int, int, int, int, int)>(0x6B41B0);
	unsigned int DX8TexturePass::setCTPipelineState(PropertyState* propertyState, int a3, int a4, unsigned int textureSetCount, int a6, int a7, int a8, int a9) {
		return NI_DX8TexturePass_setCTPipelineState(this, propertyState, a3, a4, textureSetCount, a6, a7, a8, a9);
	}

	unsigned int DX8TexturePass::setCTPipelineStateWithinStageBudget(PropertyState* propertyState, int a3, int a4, unsigned int textureSetCount, int a6, int a7, int a8, int a9) {
		const HiddenTextures hiddenTextures(propertyState ? propertyState->texture : nullptr);
		return setCTPipelineState(propertyState, a3, a4, textureSetCount, a6, a7, a8, a9);
	}

	DX8TexturePass::HiddenTextures::HiddenTextures(TexturingProperty* property) {
		// A shape with up to 8 maps pays this one compare.
		if (!property || property->maps.getEndIndex() <= STAGE_COUNT) {
			return;
		}

		// Every map with a texture counts, so the last decals go first.
		auto stageCount = 0u;
		for (auto i = 0u; i < property->maps.getEndIndex(); ++i) {
			const auto map = property->maps.storage[i];
			if (!map || !map->texture) {
				continue;
			}

			stageCount++;
			if (stageCount <= STAGE_COUNT) {
				continue;
			}

			hidden.emplace_back(map, map->texture.get());
			*reinterpret_cast<Texture**>(&map->texture) = nullptr;
		}
	}

	DX8TexturePass::HiddenTextures::~HiddenTextures() {
		for (const auto& [map, texture] : hidden) {
			*reinterpret_cast<Texture**>(&map->texture) = texture;
		}
	}
}
