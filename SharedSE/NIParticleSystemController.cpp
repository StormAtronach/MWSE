#include "NIParticleSystemController.h"

#include "ExceptionUtil.h"
#include "MemoryUtil.h"

#include "NIParticles.h"
#include "NIPerParticleData.h"
#include "NIRTTI.h"

namespace NI {
	std::span<PerParticleData> ParticleSystemController::getPerParticleData() {
		if (particleData == nullptr) {
			return {};
		}
		return { particleData, particleDataCount };
	}

	Object* ParticleSystemController::createClone() {
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR > 0 && defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY > 0 && defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto clone = _ctor(se::memory::_new<ParticleSystemController>());
		_copy(this, clone);
		giveTargetCopyOfModelData();
		return clone;
#else
		throw not_implemented_exception();
#endif
	}

	void ParticleSystemController::giveTargetCopyOfModelData() {
		if (target == nullptr || !target->isInstanceOfType(RTTIStaticPtr::NiParticles)) {
			return;
		}

		const auto particles = static_cast<Particles*>(target);
		const auto data = particles->getModelData();
		if (data) {
			particles->setModelData(data->createCopy());
		}
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::ParticleSystemController)
#endif
