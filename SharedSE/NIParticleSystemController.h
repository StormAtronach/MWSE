#pragma once

#include "NITimeController.h"
#include "NIParticleModifier.h"
#include "NIPoint3.h"

namespace NI {
	struct PerParticleData;

	struct ParticleSystemController_vTable : TimeController_vTable {
		void(__thiscall* computeStaticTargetBound)(ParticleSystemController*); // 0x48
		PerParticleData* (__thiscall* addNewParticle)(ParticleSystemController*, float, float, Matrix33*, Point3*, PerParticleData*); // 0x4C
	};
	static_assert(sizeof(ParticleSystemController_vTable) == 0x50, "NI::ParticleSystemController's vtable failed size validation");

	struct ParticleSystemController : TimeController {
		float scaledLastTime; // 0x34
		float speed; // 0x38
		float speedVariation; // 0x3C
		float declinationAngle; // 0x40
		float declinationAngleVariation; // 0x44
		float planarAngle; // 0x48
		float planarAngleVariation; // 0x4C
		Point3 initialNormal; // 0x50
		PackedColor initialColor; // 0x5C
		float initialSize; // 0x60
		float emitStartTime; // 0x64
		float emitStopTime; // 0x68
		bool resetParticleSystem; // 0x6C
		float birthRate; // 0x70
		float lifespan; // 0x74
		float lifespanVariance; // 0x78
		bool useBirthRate; // 0x7C
		bool spawnOnDeath; // 0x7D
		float emitterWidth; // 0x80
		float emitterHeight; // 0x84
		float emitterDepth; // 0x88
		Pointer<Object> emitter; // 0x8C
		unsigned short spawnGenerationsCount; // 0x90
		float spawnPercentage; // 0x94
		unsigned short spawnMultiplier; // 0x98
		float spawnSpeedChaos; // 0x9C
		float spawnDirectionChaos; // 0xA0
		unsigned short particleDataCount; // 0xA4
		unsigned short activeParticleCount; // 0xA6
		unsigned short currentParticleIndex; // 0xA8
		PerParticleData* particleData; // 0xAC
		Pointer<Object> emitterModifiers; // 0xB0
		Pointer<ParticleModifier> particleModifiers; // 0xB4
		Pointer<ParticleCollider> particleColliders; // 0xB8
		bool staticBounds; // 0xBC
		bool firstTime; // 0xBD
		float lastEmit; // 0xC0

		std::span<PerParticleData> getPerParticleData();

		Object* createClone();

		// The target gets a copy of its particle data. The old data stays with the clone of the target.
		void giveTargetCopyOfModelData();
		Pointer<ParticlesData> getTargetModelData() const;

		// A new particle only when the target's particle data has a free vertex; else nullptr.
		PerParticleData* addNewParticle(float time, float lastTime, Matrix33* targetToEmitterRotation, Point3* targetAccumulation, PerParticleData* oldParticle);
		bool targetHasFreeVertex() const;

		// Updates nothing for a particle that addNewParticle did not add.
		void updateParticle(float time, float timeModifier, PerParticleData* particle, ParticlesData* data);

#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_ADDNEWPARTICLE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_ADDNEWPARTICLE > 0
		static constexpr auto _addNewParticle = reinterpret_cast<PerParticleData*(__thiscall*)(ParticleSystemController*, float, float, Matrix33*, Point3*, PerParticleData*)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_ADDNEWPARTICLE);
#endif
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_PARTICLEUPDATE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_PARTICLEUPDATE > 0
		static constexpr auto _particleUpdate = reinterpret_cast<void(__thiscall*)(ParticleSystemController*, float, float, PerParticleData*, ParticlesData*)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_PARTICLEUPDATE);
#endif
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR > 0
		static constexpr auto _ctor = reinterpret_cast<ParticleSystemController*(__thiscall*)(ParticleSystemController*)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CTOR);
#endif
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY > 0
		static constexpr auto _copy = reinterpret_cast<void(__thiscall*)(const ParticleSystemController*, ParticleSystemController*)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_COPY);
#endif
#if defined(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CREATECLONE) && SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CREATECLONE > 0
		static constexpr auto _createClone = reinterpret_cast<Object*(__thiscall*)(ParticleSystemController*)>(SE_NI_PARTICLESYSTEMCONTROLLER_FNADDR_CREATECLONE);
#endif
	};
	static_assert(sizeof(ParticleSystemController) == 0xC4, "NI::ParticleSystemController failed size validation");

	struct BSPArrayController : ParticleSystemController {
		TArray<Pointer<Object>> arrayEmitters; // 0xC4

		Object* createClone();

		// A new particle only when the target's particle data has a free vertex; else nullptr.
		PerParticleData* addNewParticle(float time, float lastTime, Matrix33* targetToEmitterRotation, Point3* targetAccumulation, PerParticleData* oldParticle);

#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_ADDNEWPARTICLE) && SE_NI_BSPARRAYCONTROLLER_FNADDR_ADDNEWPARTICLE > 0
		static constexpr auto _addNewParticle = reinterpret_cast<PerParticleData*(__thiscall*)(BSPArrayController*, float, float, Matrix33*, Point3*, PerParticleData*)>(SE_NI_BSPARRAYCONTROLLER_FNADDR_ADDNEWPARTICLE);
#endif
#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_CTOR) && SE_NI_BSPARRAYCONTROLLER_FNADDR_CTOR > 0
		static constexpr auto _ctor = reinterpret_cast<BSPArrayController*(__thiscall*)(BSPArrayController*)>(SE_NI_BSPARRAYCONTROLLER_FNADDR_CTOR);
#endif
#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_COPY) && SE_NI_BSPARRAYCONTROLLER_FNADDR_COPY > 0
		static constexpr auto _copy = reinterpret_cast<void(__thiscall*)(const BSPArrayController*, BSPArrayController*)>(SE_NI_BSPARRAYCONTROLLER_FNADDR_COPY);
#endif
#if defined(SE_NI_BSPARRAYCONTROLLER_FNADDR_CREATECLONE) && SE_NI_BSPARRAYCONTROLLER_FNADDR_CREATECLONE > 0
		static constexpr auto _createClone = reinterpret_cast<Object*(__thiscall*)(BSPArrayController*)>(SE_NI_BSPARRAYCONTROLLER_FNADDR_CREATECLONE);
#endif
	};
	static_assert(sizeof(BSPArrayController) == 0xDC, "NI::BSPArrayController failed size validation");
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DECLARE_NI(NI::ParticleSystemController)
#endif
