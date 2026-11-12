#include "NIParticleSystemController.h"

#include "ExceptionUtil.h"
#include "MemoryUtil.h"

#include "NINode.h"
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

	void ParticleSystemController::updateParticles(float time) {
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES > 0
		const auto NI_ParticleSystemController_updateParticles = reinterpret_cast<void(__thiscall*)(ParticleSystemController*, float)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_UPDATEPARTICLES);
		NI_ParticleSystemController_updateParticles(this, time);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<ParticlesData> ParticleSystemController::getTargetModelData() const {
		if (target == nullptr || !target->isInstanceOfType(RTTIStaticPtr::NiParticles)) {
			return nullptr;
		}
		return static_cast<const Particles*>(target)->getModelData();
	}

	void ParticleSystemController::giveTargetCopyOfModelData() {
		const auto data = getTargetModelData();
		if (data) {
			static_cast<Particles*>(target)->setModelData(data->createCopy());
		}
	}

	bool ParticleSystemController::targetHasFreeVertex() const {
		const auto data = getTargetModelData();
		return data && data->activeCount < data->vertexCount;
	}

	PerParticleData* ParticleSystemController::addNewParticle(float time, float lastTime, Matrix33* targetToEmitterRotation, Point3* targetAccumulation, PerParticleData* oldParticle) {
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_ADDNEWPARTICLE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_ADDNEWPARTICLE > 0
		if (!targetHasFreeVertex()) {
			return nullptr;
		}
		return _addNewParticle(this, time, lastTime, targetToEmitterRotation, targetAccumulation, oldParticle);
#else
		throw not_implemented_exception();
#endif
	}

	void ParticleSystemController::updateParticle(float time, float timeModifier, PerParticleData* particle, ParticlesData* data) {
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_PARTICLEUPDATE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_PARTICLEUPDATE > 0
		if (particle) {
			_particleUpdate(this, time, timeModifier, particle, data);
		}
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

	Object* BSPArrayController::createClone() {
#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_CTOR) && SE_NI_BSPARRAYCONTROLLER_FNADDR_CTOR > 0 && defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_COPY) && SE_NI_BSPARRAYCONTROLLER_FNADDR_COPY > 0 && defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto clone = _ctor(se::memory::_new<BSPArrayController>());
		_copy(this, clone);

		const auto data = getTargetModelData();
		if (data && data->isPlain()) {
			giveTargetCopyOfModelData();
		}

		return clone;
#else
		throw not_implemented_exception();
#endif
	}

	PerParticleData* BSPArrayController::addNewParticle(float time, float lastTime, Matrix33* targetToEmitterRotation, Point3* targetAccumulation, PerParticleData* oldParticle) {
#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_ADDNEWPARTICLE) && SE_NI_BSPARRAYCONTROLLER_FNADDR_ADDNEWPARTICLE > 0
		if (!targetHasFreeVertex()) {
			return nullptr;
		}
		return _addNewParticle(this, time, lastTime, targetToEmitterRotation, targetAccumulation, oldParticle);
#else
		throw not_implemented_exception();
#endif
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::ParticleSystemController)
#endif
