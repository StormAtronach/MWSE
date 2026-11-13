#pragma once

#include "NITriBasedGeometry.h"
#include "NIPoint3.h"

namespace NI {
	struct ParticlesData : TriBasedGeometryData {
		float radius; // 0x38
		unsigned short activeCount; // 0x3C
		float* sizes; // 0x40

		static Pointer<ParticlesData> create(unsigned short vertexCount, Point3* vertices, Point3* normals, PackedColor* colors);
		static Pointer<ParticlesData> create(unsigned short vertexCount, bool hasNormals, bool hasColors);

		std::span<float> getSizes();
		float getLargestActiveSize() const;

		// Takes ownership of the array.
		void replaceSizes(float* sizes);
		void setActiveParticleCount(unsigned short count);

		// A copy of the same type with its own arrays.
		Pointer<ParticlesData> createCopy() const;

		// Neither auto-normal nor rotating particle data.
		bool isPlain() const;

#if defined(SE_NI_PARTICLESDATA_FNADDR_REPLACESIZES) && SE_NI_PARTICLESDATA_FNADDR_REPLACESIZES > 0
		static constexpr auto _replaceSizes = reinterpret_cast<void(__thiscall*)(ParticlesData*, float*)>(SE_NI_PARTICLESDATA_FNADDR_REPLACESIZES);
#endif
#if defined(SE_NI_PARTICLESDATA_FNADDR_SETACTIVEPARTICLECOUNT) && SE_NI_PARTICLESDATA_FNADDR_SETACTIVEPARTICLECOUNT > 0
		static constexpr auto _setActiveParticleCount = reinterpret_cast<void(__thiscall*)(ParticlesData*, unsigned short)>(SE_NI_PARTICLESDATA_FNADDR_SETACTIVEPARTICLECOUNT);
#endif
	};
	static_assert(sizeof(ParticlesData) == 0x44, "NI::ParticlesData failed size validation");

	struct AutoNormalParticlesData : ParticlesData {
		static Pointer<AutoNormalParticlesData> create(unsigned short vertexCount, Point3* vertices, PackedColor* colors);
		static Pointer<AutoNormalParticlesData> create(unsigned short vertexCount, bool hasColors);
	};
	static_assert(sizeof(AutoNormalParticlesData) == 0x44, "NI::AutoNormalParticlesData failed size validation");

	struct RotatingParticlesData : ParticlesData {
		Quaternion* rotations; // 0x44

		static Pointer<RotatingParticlesData> create(unsigned short vertexCount, Point3* vertices, Point3* normals, PackedColor* colors);

		std::span<NI::Quaternion> getRotations();

		// Takes ownership of the array.
		void replaceRotations(Quaternion* rotations);

#if defined(SE_NI_ROTATINGPARTICLESDATA_FNADDR_REPLACEROTATIONS) && SE_NI_ROTATINGPARTICLESDATA_FNADDR_REPLACEROTATIONS > 0
		static constexpr auto _replaceRotations = reinterpret_cast<void(__thiscall*)(RotatingParticlesData*, Quaternion*)>(SE_NI_ROTATINGPARTICLESDATA_FNADDR_REPLACEROTATIONS);
#endif
	};
	static_assert(sizeof(RotatingParticlesData) == 0x48, "NI::RotatingParticlesData failed size validation");

	struct Particles : TriBasedGeometry {
		AVObject* staticBoundPositionSource; // 0xAC

		Particles(ParticlesData* data);
		static Pointer<Particles> create(unsigned short vertexCount, bool hasNormals, bool hasColors);

		Pointer<ParticlesData> getModelData() const;

		// The engine's world bound, with the reach of the quads of the particles added to its radius.
		void updateWorldBound();

		// After the link: with no particle system controller, the data's bound grows to hold the active positions.
		void createRendererData();
		bool hasParticleSystemController() const;

#if defined(SE_NI_PARTICLES_FNADDR_UPDATEWORLDBOUND) && SE_NI_PARTICLES_FNADDR_UPDATEWORLDBOUND > 0
		static constexpr auto _updateWorldBound = reinterpret_cast<void(__thiscall*)(Particles*)>(SE_NI_PARTICLES_FNADDR_UPDATEWORLDBOUND);
#endif
	};
	static_assert(sizeof(Particles) == 0xB0, "NI::Particles failed size validation");

	struct AutoNormalParticles : Particles {
		AutoNormalParticles(AutoNormalParticlesData* data);
		static Pointer<AutoNormalParticles> create(unsigned short vertexCount, bool hasColors);

		Pointer<AutoNormalParticlesData> getModelData() const;
	};
	static_assert(sizeof(AutoNormalParticles) == 0xB0, "NI::AutoNormalParticles failed size validation");

	struct RotatingParticles : Particles {
		RotatingParticles(RotatingParticlesData* data);

		Pointer<RotatingParticlesData> getModelData() const;
	};
	static_assert(sizeof(RotatingParticles) == 0xB0, "NI::RotatingParticles failed size validation");
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DECLARE_NI(NI::Particles)
MWSE_SOL_CUSTOMIZED_PUSHER_DECLARE_NI(NI::AutoNormalParticles)
MWSE_SOL_CUSTOMIZED_PUSHER_DECLARE_NI(NI::RotatingParticles)
#endif
