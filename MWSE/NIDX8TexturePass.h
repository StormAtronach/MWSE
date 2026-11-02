#pragma once

#include "NIDefines.h"

#include "NIProperty.h"

namespace NI {
	struct DX8TexturePass {
		static constexpr auto STAGE_COUNT = 8u;

		// Takes the texture off every map past the first STAGE_COUNT with a texture, in the order of the maps, until the
		// scope ends. The write is raw: the texture keeps its reference count.
		struct HiddenTextures {
			HiddenTextures(TexturingProperty* property);
			~HiddenTextures();

			HiddenTextures(const HiddenTextures&) = delete;
			HiddenTextures& operator=(const HiddenTextures&) = delete;

		private:
			std::vector<std::pair<TexturingProperty::Map*, Texture*>> hidden;
		};

		unsigned int setCTPipelineState(PropertyState* propertyState, int a3, int a4, unsigned int textureSetCount, int a6, int a7, int a8, int a9);
		unsigned int setCTPipelineStateWithinStageBudget(PropertyState* propertyState, int a3, int a4, unsigned int textureSetCount, int a6, int a7, int a8, int a9);
	};
}
