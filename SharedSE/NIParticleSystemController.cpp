#include "NIParticleSystemController.h"

#include "NIPerParticleData.h"
#include "NIParticles.h"
#include "NINode.h"

#include "ExceptionUtil.h"

namespace NI {
	std::span<PerParticleData> ParticleSystemController::getPerParticleData() {
		if (particleData == nullptr) {
			return {};
		}
		return { particleData, particleDataCount };
	}

	void ParticleSystemController::updateParticles(float time) {
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES > 0
		const auto NI_ParticleSystemController_updateParticles = reinterpret_cast<void(__thiscall*)(ParticleSystemController*, float)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES);
		NI_ParticleSystemController_updateParticles(this, time);
#else
		throw not_implemented_exception();
#endif
	}

	void ParticleSystemController::updateParticlesWithQuadBound(float time) {
		updateParticles(time);
		fitBoundToQuads();
	}

	void ParticleSystemController::fitBoundToQuads() {
		if (target == nullptr || !target->isInstanceOfType(RTTIStaticPtr::NiParticles)) {
			return;
		}

		const auto particles = static_cast<Particles*>(target);
		if (particles->staticBoundPositionSource != nullptr) {
			return;
		}

		const auto data = static_cast<ParticlesData*>(particles->modelData.get());
		if (data == nullptr) {
			return;
		}

		auto scale = particles->localScale;
		if (particles->parentNode) {
			scale *= particles->parentNode->worldTransform.scale;
		}

		constexpr auto SQRT_2 = 1.41421356f;
		data->bounds.radius += data->radius * data->getLargestActiveSize() * SQRT_2 * scale;
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::ParticleSystemController)
#endif
