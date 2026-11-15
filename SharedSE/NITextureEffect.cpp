#include "NITextureEffect.h"

#include "ExceptionUtil.h"

namespace NI {
	void TextureEffect::ctor() {
#if defined(SE_NI_TEXTUREEFFECT_FNADDR_CTOR) && SE_NI_TEXTUREEFFECT_FNADDR_CTOR > 0
		reinterpret_cast<void(__thiscall*)(const DynamicEffect*)>(SE_NI_TEXTUREEFFECT_FNADDR_CTOR)(this);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<TextureEffect> TextureEffect::create() {
		return construct<TextureEffect>();
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::TextureEffect)
#endif
