#include "NIParticles.h"

#include "ExceptionUtil.h"
#include "MemoryUtil.h"

#include "NIQuaternion.h"
#include "NIRTTI.h"

namespace NI {
	Particles::Particles(ParticlesData* data) : TriBasedGeometry(data) {
#if defined(SE_NI_PARTICLES_VTBL) && SE_NI_PARTICLES_VTBL > 0
		vTable.asObject = reinterpret_cast<Object_vTable*>(SE_NI_PARTICLES_VTBL);
		staticBoundPositionSource = nullptr;
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<Particles> Particles::create(unsigned short vertexCount, bool hasNormals, bool hasColors) {
		auto data = ParticlesData::create(vertexCount, hasNormals, hasColors);
		auto geom = new Particles(data);

		return geom;
	}

	Pointer<ParticlesData> Particles::getModelData() const {
		return static_cast<ParticlesData*>(modelData.get());
	}

	AutoNormalParticles::AutoNormalParticles(AutoNormalParticlesData* data) : Particles(data) {
#if defined(SE_NI_AUTONORMALPARTICLES_VTBL) && SE_NI_AUTONORMALPARTICLES_VTBL > 0
		vTable.asObject = reinterpret_cast<Object_vTable*>(SE_NI_AUTONORMALPARTICLES_VTBL);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<AutoNormalParticles> AutoNormalParticles::create(unsigned short vertexCount, bool hasColors) {
		auto data = AutoNormalParticlesData::create(vertexCount, hasColors);
		auto geom = new AutoNormalParticles(data);

		return geom;
	}

	Pointer<AutoNormalParticlesData> AutoNormalParticles::getModelData() const {
		return static_cast<AutoNormalParticlesData*>(modelData.get());
	}

	Pointer<ParticlesData> ParticlesData::create(unsigned short _vertexCount, Point3* _vertices, Point3* _normals, PackedColor* _colors) {
#if defined(SE_NI_PARTICLESDATA_FNADDR_CTOR) && SE_NI_PARTICLESDATA_FNADDR_CTOR > 0 && defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto NI_ParticlesData_ctor = reinterpret_cast<void(__thiscall*)(ParticlesData*, unsigned short, Point3*, Point3*, PackedColor*)>(SE_NI_PARTICLESDATA_FNADDR_CTOR);
		auto ptr = se::memory::_new<ParticlesData>();
		NI_ParticlesData_ctor(ptr, _vertexCount, _vertices, _normals, _colors);
		return ptr;
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<ParticlesData> ParticlesData::create(unsigned short vertexCount, bool hasNormals, bool hasColors) {
#if defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		Point3* vertices = se::memory::_new<Point3>(vertexCount);
		ZeroMemory(vertices, sizeof(Point3) * vertexCount);

		Point3* normals = nullptr;
		if (hasNormals) {
			normals = se::memory::_new<Point3>(vertexCount);
			ZeroMemory(normals, sizeof(Point3) * vertexCount);
		}

		PackedColor* colors = nullptr;
		if (hasColors) {
			colors = se::memory::_new<PackedColor>(vertexCount);
			ZeroMemory(colors, sizeof(PackedColor) * vertexCount);
		}

		return create(vertexCount, vertices, normals, colors);
#else
		throw not_implemented_exception();
#endif
	}

	std::span<float> ParticlesData::getSizes() {
		if (sizes) {
			return std::span(sizes, vertexCount);
		}
		return {};
	}

	float ParticlesData::getLargestActiveSize() const {
		if (sizes == nullptr) {
			return 1.0f;
		}

		auto largest = 0.0f;
		for (auto i = 0u; i < activeCount; ++i) {
			largest = std::max(largest, sizes[i]);
		}
		return largest;
	}

	void ParticlesData::replaceSizes(float* _sizes) {
#if defined(SE_NI_PARTICLESDATA_FNADDR_REPLACESIZES) && SE_NI_PARTICLESDATA_FNADDR_REPLACESIZES > 0
		_replaceSizes(this, _sizes);
#else
		throw not_implemented_exception();
#endif
	}

	void ParticlesData::setActiveParticleCount(unsigned short count) {
#if defined(SE_NI_PARTICLESDATA_FNADDR_SETACTIVEPARTICLECOUNT) && SE_NI_PARTICLESDATA_FNADDR_SETACTIVEPARTICLECOUNT > 0
		_setActiveParticleCount(this, count);
#else
		throw not_implemented_exception();
#endif
	}

	bool ParticlesData::isPlain() const {
		return !isInstanceOfType(RTTIStaticPtr::NiAutoNormalParticlesData) && !isInstanceOfType(RTTIStaticPtr::NiRotatingParticlesData);
	}

	Pointer<ParticlesData> ParticlesData::createCopy() const {
#if defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto copyArray = [this]<typename T>(const T* source) -> T* {
			if (source == nullptr) {
				return nullptr;
			}
			const auto copy = se::memory::_new<T>(vertexCount);
			std::copy_n(source, vertexCount, copy);
			return copy;
		};

		const auto vertices = copyArray(vertex);
		const auto colors = copyArray(color);

		Pointer<ParticlesData> copy;
		if (isInstanceOfType(RTTIStaticPtr::NiAutoNormalParticlesData)) {
			copy = AutoNormalParticlesData::create(vertexCount, vertices, colors);
		}
		else if (isInstanceOfType(RTTIStaticPtr::NiRotatingParticlesData)) {
			const auto rotatingCopy = RotatingParticlesData::create(vertexCount, vertices, copyArray(normal), colors);
			const auto rotations = static_cast<const RotatingParticlesData*>(this)->rotations;
			if (rotations) {
				rotatingCopy->replaceRotations(copyArray(rotations));
			}
			copy = rotatingCopy;
		}
		else {
			copy = create(vertexCount, vertices, copyArray(normal), colors);
		}

		copy->radius = radius;
		copy->setActiveParticleCount(activeCount);
		if (sizes) {
			copy->replaceSizes(copyArray(sizes));
		}

		return copy;
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<AutoNormalParticlesData> AutoNormalParticlesData::create(unsigned short _vertexCount, Point3* _vertices, PackedColor* _colors) {
#if defined(SE_NI_AUTONORMALPARTICLESDATA_FNADDR_CTOR) && SE_NI_AUTONORMALPARTICLESDATA_FNADDR_CTOR > 0 && defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto NI_AutoNormalParticlesData_ctor = reinterpret_cast<void(__thiscall*)(AutoNormalParticlesData*, unsigned short, Point3*, PackedColor*)>(SE_NI_AUTONORMALPARTICLESDATA_FNADDR_CTOR);
		auto ptr = se::memory::_new<AutoNormalParticlesData>();
		NI_AutoNormalParticlesData_ctor(ptr, _vertexCount, _vertices, _colors);
		return ptr;
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<AutoNormalParticlesData> AutoNormalParticlesData::create(unsigned short vertexCount, bool hasColors) {
#if defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		Point3* vertices = se::memory::_new<Point3>(vertexCount);
		ZeroMemory(vertices, sizeof(Point3) * vertexCount);

		PackedColor* colors = nullptr;
		if (hasColors) {
			colors = se::memory::_new<PackedColor>(vertexCount);
			ZeroMemory(colors, sizeof(PackedColor) * vertexCount);
		}

		return create(vertexCount, vertices, colors);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<RotatingParticlesData> RotatingParticlesData::create(unsigned short _vertexCount, Point3* _vertices, Point3* _normals, PackedColor* _colors) {
#if defined(SE_NI_ROTATINGPARTICLESDATA_FNADDR_CTOR) && SE_NI_ROTATINGPARTICLESDATA_FNADDR_CTOR > 0 && defined(SE_MEMORY_FNADDR_NEW) && SE_MEMORY_FNADDR_NEW > 0
		const auto NI_RotatingParticlesData_ctor = reinterpret_cast<void(__thiscall*)(RotatingParticlesData*, unsigned short, Point3*, Point3*, PackedColor*)>(SE_NI_ROTATINGPARTICLESDATA_FNADDR_CTOR);
		auto ptr = se::memory::_new<RotatingParticlesData>();
		NI_RotatingParticlesData_ctor(ptr, _vertexCount, _vertices, _normals, _colors);
		return ptr;
#else
		throw not_implemented_exception();
#endif
	}

	std::span<NI::Quaternion> RotatingParticlesData::getRotations() {
		if (rotations) {
			return std::span(rotations, vertexCount);
		}
		return {};
	}

	void RotatingParticlesData::replaceRotations(Quaternion* _rotations) {
#if defined(SE_NI_ROTATINGPARTICLESDATA_FNADDR_REPLACEROTATIONS) && SE_NI_ROTATINGPARTICLESDATA_FNADDR_REPLACEROTATIONS > 0
		_replaceRotations(this, _rotations);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<RotatingParticlesData> RotatingParticles::getModelData() const {
		return static_cast<RotatingParticlesData*>(modelData.get());
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::Particles)
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::AutoNormalParticles)
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::RotatingParticles)
#endif
