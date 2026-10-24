#include "NIParticleSystemController.h"

#include "NIGeometry.h"
#include "NIPerParticleData.h"
#include "MemoryUtil.h"

namespace NI {
	std::span<PerParticleData> ParticleSystemController::getPerParticleData() {
		if (particleData == nullptr) {
			return {};
		}
		return { particleData, particleDataCount };
	}

#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY > 0
	void ParticleSystemController::copy(ParticleSystemController* to) const {
		_copy(this, to);
	}
#endif

#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CREATECLONE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CREATECLONE > 0 && defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR > 0
	Object* ParticleSystemController::createCloneNoLeak() {
		const auto geometry = static_cast<Geometry*>(target);
		const auto data = geometry ? geometry->modelData.get() : nullptr;
		if (data && !data->isInstanceOfType(RTTIStaticPtr::NiAutoNormalParticlesData) && !data->isInstanceOfType(RTTIStaticPtr::NiRotatingParticlesData)) {
			const auto clone = _ctor(se::memory::_new<ParticleSystemController>());
			copy(clone);
			return clone;
		}
		return _createClone(this);
	}
#endif
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::ParticleSystemController)
#endif
