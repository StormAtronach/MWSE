#include "PatchWaterVolumes.h"

#include "Log.h"
#include "MemoryUtil.h"

#include "TES3ActorAnimationController.h"
#include "TES3Cell.h"
#include "TES3CombatSession.h"
#include "TES3DataHandler.h"
#include "TES3MobileActor.h"
#include "TES3MobilePlayer.h"
#include "TES3Reference.h"
#include "TES3WaterController.h"
#include "TES3WorldController.h"

#include "NICamera.h"
#include "NINode.h"
#include "NIRTTI.h"
#include "NITriShape.h"
#include "NITriShapeData.h"

namespace mwse::patch::waterVolumes {

	//
	// Registry.
	//

	static std::vector<Volume> volumes;
	static int nextVolumeId = 1;
	static bool installed = false;
	static DWORD mainThreadId = 0;

	// Read from assembly. True while at least one volume exists.
	static bool anyVolumes = false;

	// Layout shared with the renderer's exported setter.
	struct ExportedVolume {
		float min[3];
		float max[3];
	};

	// The renderer is told about one volume at most: the one the camera is in, as a box whose
	// top is the surface at the camera.
	static int rendererVolumeId = 0;
	static float rendererSurface = 0.0f;

	static void setRendererVolume(const Volume* volume, float surface, float floor) {
		const auto id = volume ? volume->id : 0;
		if (id == rendererVolumeId && (id == 0 || std::abs(surface - rendererSurface) < 0.5f)) {
			return;
		}
		rendererVolumeId = id;
		rendererSurface = surface;

		const auto renderer = GetModuleHandleA("d3d8.dll");
		if (renderer == NULL) {
			return;
		}
		const auto setter = reinterpret_cast<void(__cdecl*)(const ExportedVolume*, unsigned int)>(GetProcAddress(renderer, "MGE_WaterVolumesSet"));
		if (setter == nullptr) {
			return;
		}

		if (volume) {
			const ExportedVolume exported = { { volume->min.x, volume->min.y, floor }, { volume->max.x, volume->max.y, surface } };
			setter(&exported, 1);
		}
		else {
			setter(nullptr, 0);
		}
	}

	static int store(Volume&& volume) {
		if (mainThreadId == 0) {
			mainThreadId = GetCurrentThreadId();
		}
		volume.id = nextVolumeId++;
		volumes.push_back(std::move(volume));
		anyVolumes = true;
		return volumes.back().id;
	}

	int add(const NI::Point3& min, const NI::Point3& max) {
		if (!installed) {
			return 0;
		}

		Volume volume = {};
		volume.min = NI::Point3(std::min(min.x, max.x), std::min(min.y, max.y), std::min(min.z, max.z));
		volume.max = NI::Point3(std::max(min.x, max.x), std::max(min.y, max.y), std::max(min.z, max.z));
		volume.depth = volume.max.z - volume.min.z;
		return store(std::move(volume));
	}

	static void collectFootprint(NI::AVObject* object, Volume& volume) {
		if (object == nullptr || object->getAppCulled()) {
			return;
		}

		if (object->isInstanceOfType(NI::RTTIStaticPtr::NiTriShape)) {
			const auto shape = static_cast<NI::TriShape*>(object);
			const auto data = shape->getModelData();
			if (data == nullptr || data->vertex == nullptr || data->triangleList == nullptr) {
				return;
			}

			const auto triangleCount = data->getActiveTriangleCount();
			for (auto i = 0u; i < triangleCount; ++i) {
				FootprintTriangle triangle = {};
				const auto& indices = data->triangleList[i].vertices;
				triangle.a = shape->worldTransform * data->vertex[indices[0]];
				triangle.b = shape->worldTransform * data->vertex[indices[1]];
				triangle.c = shape->worldTransform * data->vertex[indices[2]];

				// A triangle seen edge-on from above covers no area.
				triangle.denominator = (triangle.b.y - triangle.c.y) * (triangle.a.x - triangle.c.x) + (triangle.c.x - triangle.b.x) * (triangle.a.y - triangle.c.y);
				if (std::abs(triangle.denominator) < 1e-6f) {
					continue;
				}
				volume.footprint.push_back(triangle);
			}
		}
		else if (object->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
			for (const auto& child : static_cast<NI::Node*>(object)->children) {
				collectFootprint(child.get(), volume);
			}
		}
	}

	// Spreads the triangles over a grid so that a lookup tests only the few near the point.
	static void buildGrid(Volume& volume) {
		const auto count = volume.footprint.size();
		volume.gridSize = std::clamp(static_cast<unsigned int>(std::ceil(std::sqrt(static_cast<float>(count)))), 1u, 64u);
		volume.grid.assign(volume.gridSize * volume.gridSize, {});

		const auto width = std::max(volume.max.x - volume.min.x, 1.0f);
		const auto height = std::max(volume.max.y - volume.min.y, 1.0f);
		volume.gridScaleX = volume.gridSize / width;
		volume.gridScaleY = volume.gridSize / height;

		const auto cellOf = [&](float value, float origin, float scale) {
			return std::clamp(static_cast<int>((value - origin) * scale), 0, static_cast<int>(volume.gridSize) - 1);
		};
		for (auto i = 0u; i < count; ++i) {
			const auto& t = volume.footprint[i];
			const auto x0 = cellOf(std::min({ t.a.x, t.b.x, t.c.x }), volume.min.x, volume.gridScaleX);
			const auto x1 = cellOf(std::max({ t.a.x, t.b.x, t.c.x }), volume.min.x, volume.gridScaleX);
			const auto y0 = cellOf(std::min({ t.a.y, t.b.y, t.c.y }), volume.min.y, volume.gridScaleY);
			const auto y1 = cellOf(std::max({ t.a.y, t.b.y, t.c.y }), volume.min.y, volume.gridScaleY);
			for (auto y = y0; y <= y1; ++y) {
				for (auto x = x0; x <= x1; ++x) {
					volume.grid[y * volume.gridSize + x].push_back(i);
				}
			}
		}
	}

	int addFromNode(NI::AVObject* node, float depth) {
		if (!installed || node == nullptr) {
			return 0;
		}

		Volume volume = {};
		collectFootprint(node, volume);
		if (volume.footprint.empty()) {
			return 0;
		}

		volume.min = volume.footprint[0].a;
		volume.max = volume.footprint[0].a;
		for (const auto& t : volume.footprint) {
			for (const auto& corner : { t.a, t.b, t.c }) {
				volume.min.x = std::min(volume.min.x, corner.x);
				volume.min.y = std::min(volume.min.y, corner.y);
				volume.min.z = std::min(volume.min.z, corner.z);
				volume.max.x = std::max(volume.max.x, corner.x);
				volume.max.y = std::max(volume.max.y, corner.y);
				volume.max.z = std::max(volume.max.z, corner.z);
			}
		}
		volume.depth = std::max(depth, 0.0f);
		// The lowest point any part of the volume reaches.
		volume.min.z -= volume.depth;

		buildGrid(volume);
		return store(std::move(volume));
	}

	bool remove(int id) {
		const auto itt = std::find_if(volumes.begin(), volumes.end(), [id](const Volume& v) { return v.id == id; });
		if (itt == volumes.end()) {
			return false;
		}
		volumes.erase(itt);
		anyVolumes = !volumes.empty();
		if (id == rendererVolumeId) {
			setRendererVolume(nullptr, 0.0f, 0.0f);
		}
		return true;
	}

	void clear() {
		volumes.clear();
		anyVolumes = false;
		setRendererVolume(nullptr, 0.0f, 0.0f);
	}

	const std::vector<Volume>& getVolumes() {
		return volumes;
	}

	bool isInstalled() {
		return installed;
	}

	// Heights closer together than this are one layer of the surface. Meshes often carry the
	// surface twice, a little apart, for two layers of texture.
	constexpr auto LAYER_TOLERANCE = 8.0f;

	// The surface and the floor of the volume's water at a position, if there is any.
	// Where one layer of triangles lies over the position, the water reaches depth below it.
	// Where several do, the lowest is the floor, and the water under each of the others reaches
	// down to the layer below it. A position above the water gets the surface under it.
	static bool waterAt(const Volume& volume, const NI::Point3* position, bool ignoreHeight, float& out_surface, float& out_floor) {
		if (volume.footprint.empty()) {
			out_surface = volume.max.z;
			out_floor = volume.min.z;
			return true;
		}

		const auto x = position->x;
		const auto y = position->y;
		const auto cellX = std::clamp(static_cast<int>((x - volume.min.x) * volume.gridScaleX), 0, static_cast<int>(volume.gridSize) - 1);
		const auto cellY = std::clamp(static_cast<int>((y - volume.min.y) * volume.gridScaleY), 0, static_cast<int>(volume.gridSize) - 1);

		constexpr auto EDGE_TOLERANCE = -1e-4f;
		constexpr auto MAX_HEIGHTS = 32u;
		float heights[MAX_HEIGHTS];
		auto count = 0u;
		for (const auto index : volume.grid[cellY * volume.gridSize + cellX]) {
			const auto& t = volume.footprint[index];
			const auto w1 = ((t.b.y - t.c.y) * (x - t.c.x) + (t.c.x - t.b.x) * (y - t.c.y)) / t.denominator;
			const auto w2 = ((t.c.y - t.a.y) * (x - t.c.x) + (t.a.x - t.c.x) * (y - t.c.y)) / t.denominator;
			const auto w3 = 1.0f - w1 - w2;
			if (w1 < EDGE_TOLERANCE || w2 < EDGE_TOLERANCE || w3 < EDGE_TOLERANCE) {
				continue;
			}
			heights[count++] = w1 * t.a.z + w2 * t.b.z + w3 * t.c.z;
			if (count == MAX_HEIGHTS) {
				break;
			}
		}
		if (count == 0) {
			return false;
		}

		const auto lowest = *std::min_element(heights, heights + count);
		const auto highest = *std::max_element(heights, heights + count);
		if (highest - lowest <= LAYER_TOLERANCE) {
			out_surface = highest;
			out_floor = highest - volume.depth;
			return ignoreHeight || position->z >= out_floor;
		}

		// The lowest height at or above the position belongs to the layer whose water it is in.
		auto above = highest;
		auto anyAbove = false;
		for (auto i = 0u; i < count; ++i) {
			if (heights[i] >= position->z && heights[i] <= above) {
				above = heights[i];
				anyAbove = true;
			}
		}
		if (ignoreHeight || !anyAbove) {
			out_surface = highest;
			out_floor = lowest;
			return true;
		}

		// The top of that layer is the surface; the top of the layer under it is the floor.
		auto surface = above;
		auto floor = 0.0f;
		auto anyFloor = false;
		for (auto i = 0u; i < count; ++i) {
			const auto z = heights[i];
			if (z > surface && z <= above + LAYER_TOLERANCE) {
				surface = z;
			}
			if (z < above - LAYER_TOLERANCE && (!anyFloor || z > floor)) {
				floor = z;
				anyFloor = true;
			}
		}
		if (!anyFloor) {
			return false;
		}
		out_surface = surface;
		out_floor = floor;
		return true;
	}

	static const Volume* findVolume(const NI::Point3* position, bool ignoreHeight, float& out_surface, float& out_floor) {
		const Volume* found = nullptr;
		for (const auto& volume : volumes) {
			if (position->x < volume.min.x || position->x > volume.max.x) continue;
			if (position->y < volume.min.y || position->y > volume.max.y) continue;
			if (!ignoreHeight && position->z < volume.min.z) continue;

			float surface = 0.0f, floor = 0.0f;
			if (!waterAt(volume, position, ignoreHeight, surface, floor)) continue;
			if (found != nullptr && surface <= out_surface) continue;

			found = &volume;
			out_surface = surface;
			out_floor = floor;
		}
		return found;
	}

	static bool findSurface(const NI::Point3* position, bool ignoreHeight, float& out_surface) {
		float floor = 0.0f;
		return findVolume(position, ignoreHeight, out_surface, floor) != nullptr;
	}

	std::optional<float> getSurfaceAt(const NI::Point3& position) {
		float surface = 0.0f;
		if (findSurface(&position, false, surface)) {
			return surface;
		}
		return {};
	}

	//
	// Subject tracking. Each hooked function names the point its water queries are about.
	//

	struct Subject {
		const NI::Point3* position;
		bool ignoreHeight;
	};

	enum class SubjectKind : DWORD {
		ThisMobile,
		Arg0Mobile,
		Arg0Position,
		Arg1Position,
		AnimationController,
		CombatSession,
		Camera,
		Player,
		Arg0Arg1XY,
	};

	struct Frame {
		DWORD returnAddress;
		DWORD stackPointer;
		Subject previous;
		NI::Point3 storage;
	};

	struct SavedRegisters {
		DWORD edi, esi, ebp, esp, ebx, edx, ecx, eax;
	};

	constexpr auto MAX_FRAMES = 64u;
	static Frame frames[MAX_FRAMES];
	static unsigned int frameCount = 0;
	static Subject subject = { nullptr, false };
	static DWORD exitStub = 0;

	static Subject subjectFromMobile(const TES3::MobileObject* mobile) {
		if (mobile == nullptr || mobile->reference == nullptr) {
			return { nullptr, false };
		}
		return { &mobile->reference->position, false };
	}

	static Subject resolveSubject(SubjectKind kind, DWORD ecx, const DWORD* stack, Frame& frame) {
		switch (kind) {
		case SubjectKind::ThisMobile:
			return subjectFromMobile(reinterpret_cast<const TES3::MobileObject*>(ecx));
		case SubjectKind::Arg0Mobile:
			return subjectFromMobile(reinterpret_cast<const TES3::MobileObject*>(stack[1]));
		case SubjectKind::Arg0Position:
			return { reinterpret_cast<const NI::Point3*>(stack[1]), false };
		case SubjectKind::Arg1Position:
			return { reinterpret_cast<const NI::Point3*>(stack[2]), false };
		case SubjectKind::AnimationController:
			return subjectFromMobile(reinterpret_cast<const TES3::ActorAnimationController*>(ecx)->mobileActor);
		case SubjectKind::CombatSession:
			return subjectFromMobile(reinterpret_cast<const TES3::CombatSession*>(ecx)->parentActor);
		case SubjectKind::Camera: {
			const auto worldController = TES3::WorldController::get();
			const auto camera = worldController ? worldController->worldCamera.cameraData.camera.get() : nullptr;
			if (camera == nullptr) {
				return { nullptr, false };
			}
			return { &camera->worldTransform.translation, false };
		}
		case SubjectKind::Player: {
			const auto worldController = TES3::WorldController::get();
			return subjectFromMobile(worldController ? worldController->getMobilePlayer() : nullptr);
		}
		case SubjectKind::Arg0Arg1XY:
			memcpy(&frame.storage.x, &stack[1], sizeof(float));
			memcpy(&frame.storage.y, &stack[2], sizeof(float));
			return { &frame.storage, true };
		}
		return { nullptr, false };
	}

	static void __stdcall onEnter(SubjectKind kind, SavedRegisters* registers) {
		if (!anyVolumes || GetCurrentThreadId() != mainThreadId) {
			return;
		}

		// Frames at or below this stack position never returned through the exit stub.
		while (frameCount > 0 && frames[frameCount - 1].stackPointer <= registers->esp) {
			frameCount--;
			subject = frames[frameCount].previous;
		}
		if (frameCount == MAX_FRAMES) {
			return;
		}

		const auto stack = reinterpret_cast<DWORD*>(registers->esp);
		auto& frame = frames[frameCount++];
		frame.returnAddress = stack[0];
		frame.stackPointer = registers->esp;
		frame.previous = subject;
		subject = resolveSubject(kind, registers->ecx, stack, frame);
		stack[0] = exitStub;
	}

	static DWORD __stdcall onLeave(DWORD stackPointer) {
		// The returning frame is the outermost one below the current stack position.
		while (frameCount > 1 && frames[frameCount - 2].stackPointer < stackPointer) {
			frameCount--;
		}
		frameCount--;
		subject = frames[frameCount].previous;
		return frames[frameCount].returnAddress;
	}

	static float adjustLevel(float base) {
		if (!anyVolumes || subject.position == nullptr || GetCurrentThreadId() != mainThreadId) {
			return base;
		}
		float surface = 0.0f;
		if (findSurface(subject.position, subject.ignoreHeight, surface) && surface > base) {
			return surface;
		}
		return base;
	}

	//
	// Replacement functions.
	//

	// Stands in for the interior cell at call sites that skip the level query in exteriors.
	alignas(4) static BYTE proxyCell[sizeof(TES3::Cell)] = {};

	const auto TES3_getWaterMinLevel = reinterpret_cast<float(__cdecl*)()>(0x51D760);
	const auto TES3_Cell_getWaterLevel = reinterpret_cast<float(__thiscall*)(const void*)>(0x4E28B0);

	static float __cdecl getWaterMinLevel() {
		return adjustLevel(TES3_getWaterMinLevel());
	}

	static float __fastcall cellGetWaterLevel(const void* cell) {
		const auto base = (cell == proxyCell) ? 0.0f : TES3_Cell_getWaterLevel(cell);
		return adjustLevel(base);
	}

	const auto TES3_WeatherController_updateUnderwaterState = reinterpret_cast<void(__thiscall*)(void*, float, float)>(0x440AF0);

	// The underwater state is decided against the height of the water plane node, so a camera
	// inside a volume is reported relative to that node.
	static void __fastcall updateUnderwaterState(void* weatherController, DWORD _UNUSED_, float cameraZ, float waterLevel) {
		float surface = 0.0f, floor = 0.0f;
		const auto volume = (anyVolumes && subject.position != nullptr) ? findVolume(subject.position, false, surface, floor) : nullptr;
		setRendererVolume(volume, surface, floor);
		if (volume != nullptr) {
			const auto dataHandler = TES3::DataHandler::get();
			const auto plane = dataHandler && dataHandler->waterController ? dataHandler->waterController->waterPlane : nullptr;
			if (plane != nullptr) {
				cameraZ = plane->worldTransform.translation.z + (subject.position->z < surface ? -1.0f : 1.0f);
			}
		}
		TES3_WeatherController_updateUnderwaterState(weatherController, cameraZ, waterLevel);
	}

	static bool __cdecl isPointUnderwater(const NI::Point3* position) {
		const auto previous = subject;
		subject = { position, false };
		const auto level = adjustLevel(TES3_getWaterMinLevel());
		subject = previous;
		return level > position->z;
	}

	static __declspec(naked) void getInteriorCellOrProxy() {
		__asm {
			mov ecx, [eax + 0xAC]
			test ecx, ecx
			jnz done
			cmp byte ptr [anyVolumes], 0
			je done
			mov ecx, offset proxyCell
		done:
			ret
		}
	}

	//
	// Hook tables.
	//

	// Call sites of the global water level query (0x51D760).
	static const DWORD globalLevelCallSites[] = {
		0x466AAA, 0x466B96,                                // waterwalk check
		0x523EEA, 0x523F0D,                                // actor scene graph update
		0x5243F3, 0x52451B, 0x52462D, 0x524799,            // actor simulation
		0x52535D, 0x5254B8, 0x52567C, 0x5257BA, 0x5258A7,  // collision resolution
		0x525D36,                                          // movement collision
		0x526CD1, 0x527464,                                // swim/fly movement, surfacing for air
		0x5287DA, 0x528935, 0x528BC0, 0x528EA9,            // destination checks
		0x528FAA,                                          // voiceover
		0x5293F8, 0x5294F6, 0x5295B0,                      // distance to surface, foot point
		0x529A42, 0x529AB6,                                // breathing
		0x52D502, 0x52D511,                                // standing position check
		0x53771A,                                          // combat weighting
		0x53B553, 0x53B5D7,                                // distance below water, swim at destination
		0x53E973, 0x53E9A7,                                // movement physics
		0x560C1B,                                          // projectile water collision
		// 0x507986: script instruction, left alone.
		// 0x53B4A0: the whole function is replaced by isPointUnderwater.
	};

	// Call sites of the cell water level query (0x4E28B0).
	static const DWORD cellLevelCallSites[] = {
		0x410394, 0x410527,            // environment update, camera
		0x441473,                      // weather particles
		0x48A598,                      // ambient water sound
		0x51C205, 0x51C386,            // ripples
		0x522C82, 0x525B29, 0x53B4E8,  // actor collision, waterwalking
		0x5523A2, 0x5523E3, 0x5524C4,  // walking
		0x552B67, 0x552BA6,            // falling
		0x552E5B,                      // swimming
		// 0x5079BF, 0x507A0F: script instructions, left alone.
		// 0x51BA97: water plane placement, left alone.
		// 0x51D77E: inside the global query, already covered by its call sites.
	};

	// Call sites of the underwater state update (0x440AF0).
	static const DWORD underwaterStateCallSites[] = {
		0x4103A4, 0x410537,
	};

	// Loads of DataHandler::currentInteriorCell that precede a null check and a level query.
	static const DWORD interiorCellLoadSites[] = {
		0x51C373, 0x522C6A, 0x552E43,
	};

	struct EntryHook {
		DWORD address;
		BYTE length;
		BYTE expected[9];
		SubjectKind kind;
	};

	static const EntryHook entryHooks[] = {
		{ 0x4100D0, 6, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C }, SubjectKind::Camera },
		{ 0x466A60, 6, { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 }, SubjectKind::Arg0Mobile },
		{ 0x48A560, 5, { 0x83, 0xEC, 0x50, 0x53, 0x56 }, SubjectKind::Player },
		{ 0x51C1E0, 5, { 0x83, 0xEC, 0x18, 0x53, 0x55 }, SubjectKind::Arg0Arg1XY },
		{ 0x523DA0, 5, { 0x83, 0xEC, 0x1C, 0x53, 0x56 }, SubjectKind::ThisMobile },
		{ 0x524070, 6, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, SubjectKind::ThisMobile },
		{ 0x525230, 6, { 0x83, 0xEC, 0x10, 0x56, 0x8B, 0xF1 }, SubjectKind::ThisMobile },
		{ 0x5259F0, 5, { 0xA1, 0xE0, 0x67, 0x7C, 0x00 }, SubjectKind::ThisMobile },
		{ 0x526BB0, 6, { 0x83, 0xEC, 0x28, 0x56, 0x8B, 0xF1 }, SubjectKind::ThisMobile },
		{ 0x527410, 6, { 0x83, 0xEC, 0x18, 0x56, 0x33, 0xC0 }, SubjectKind::ThisMobile },
		{ 0x5287D0, 5, { 0x83, 0xEC, 0x08, 0x53, 0x56 }, SubjectKind::Arg0Position },
		{ 0x528870, 6, { 0x83, 0xEC, 0x1C, 0x56, 0x8B, 0xF1 }, SubjectKind::ThisMobile },
		{ 0x5289D0, 7, { 0x83, 0xEC, 0x28, 0x8B, 0x44, 0x24, 0x2C }, SubjectKind::Arg0Position },
		{ 0x528D30, 6, { 0x83, 0xEC, 0x1C, 0x53, 0x8B, 0xD9 }, SubjectKind::ThisMobile },
		{ 0x528F80, 7, { 0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x0C }, SubjectKind::ThisMobile },
		{ 0x5293B0, 6, { 0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1 }, SubjectKind::ThisMobile },
		{ 0x529540, 6, { 0x57, 0x8B, 0xF9, 0x8B, 0x4F, 0x14 }, SubjectKind::ThisMobile },
		{ 0x5299F0, 7, { 0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x14 }, SubjectKind::ThisMobile },
		{ 0x52D420, 6, { 0x83, 0xEC, 0x08, 0x55, 0x8B, 0xE9 }, SubjectKind::ThisMobile },
		{ 0x5375A0, 5, { 0x83, 0xEC, 0x44, 0x53, 0x55 }, SubjectKind::CombatSession },
		{ 0x53B4C0, 6, { 0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C }, SubjectKind::Arg0Mobile },
		{ 0x53B500, 9, { 0x83, 0xEC, 0x08, 0x8B, 0x0D, 0xE0, 0x67, 0x7C, 0x00 }, SubjectKind::Arg0Position },
		{ 0x53B580, 9, { 0x83, 0xEC, 0x08, 0x8B, 0x0D, 0xE0, 0x67, 0x7C, 0x00 }, SubjectKind::Arg1Position },
		{ 0x53E270, 6, { 0x83, 0xEC, 0x70, 0x56, 0x8B, 0xF1 }, SubjectKind::AnimationController },
		{ 0x560BE0, 5, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, SubjectKind::ThisMobile },
		{ 0x5679E0, 6, { 0x83, 0xEC, 0x28, 0x56, 0x8B, 0xF1 }, SubjectKind::ThisMobile },
		{ 0x573790, 5, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, SubjectKind::ThisMobile },
		{ 0x574DA0, 6, { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x10 }, SubjectKind::ThisMobile },
	};

	static const BYTE interiorCellLoadBytes[] = { 0x8B, 0x88, 0xAC, 0x00, 0x00, 0x00 };

	//
	// Installation.
	//

	static bool isCallTo(DWORD address, DWORD target) {
		if (*reinterpret_cast<const BYTE*>(address) != 0xE8) {
			return false;
		}
		return se::memory::getCallAddress(address) == target;
	}

	static bool verify() {
		bool ok = true;
		for (const auto site : globalLevelCallSites) {
			if (!isCallTo(site, 0x51D760)) {
				log::getLog() << "[MWSE] Water volumes: unexpected code at call site 0x" << std::hex << site << std::dec << std::endl;
				ok = false;
			}
		}
		if (!isCallTo(0x53B4A0, 0x51D760)) {
			log::getLog() << "[MWSE] Water volumes: unexpected code at 0x53b4a0" << std::endl;
			ok = false;
		}
		for (const auto site : cellLevelCallSites) {
			if (!isCallTo(site, 0x4E28B0)) {
				log::getLog() << "[MWSE] Water volumes: unexpected code at call site 0x" << std::hex << site << std::dec << std::endl;
				ok = false;
			}
		}
		for (const auto site : underwaterStateCallSites) {
			if (!isCallTo(site, 0x440AF0)) {
				log::getLog() << "[MWSE] Water volumes: unexpected code at call site 0x" << std::hex << site << std::dec << std::endl;
				ok = false;
			}
		}
		for (const auto site : interiorCellLoadSites) {
			if (memcmp(reinterpret_cast<const void*>(site), interiorCellLoadBytes, sizeof(interiorCellLoadBytes)) != 0) {
				log::getLog() << "[MWSE] Water volumes: unexpected code at load site 0x" << std::hex << site << std::dec << std::endl;
				ok = false;
			}
		}
		for (const auto& hook : entryHooks) {
			if (memcmp(reinterpret_cast<const void*>(hook.address), hook.expected, hook.length) != 0) {
				log::getLog() << "[MWSE] Water volumes: unexpected code at function 0x" << std::hex << hook.address << std::dec << std::endl;
				ok = false;
			}
		}
		return ok;
	}

	static BYTE* emitByte(BYTE* at, BYTE value) {
		*at = value;
		return at + 1;
	}

	static BYTE* emitDword(BYTE* at, DWORD value) {
		memcpy(at, &value, sizeof(value));
		return at + sizeof(value);
	}

	static BYTE* emitRelative(BYTE* at, BYTE opcode, DWORD target) {
		at = emitByte(at, opcode);
		return emitDword(at, target - (reinterpret_cast<DWORD>(at) + 4));
	}

	bool install() {
		if (installed) {
			return true;
		}
		if (!verify()) {
			log::getLog() << "[MWSE] Water volumes: not installed." << std::endl;
			return false;
		}

		auto code = static_cast<BYTE*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (code == nullptr) {
			return false;
		}

		// HasWater | IsInterior, so the proxy passes the flag checks ahead of the level query.
		proxyCell[offsetof(TES3::Cell, cellFlags)] = 0x3;

		// Exit stub: restores the subject and returns to the original caller.
		exitStub = reinterpret_cast<DWORD>(code);
		code = emitByte(code, 0x50);                             // push eax
		code = emitByte(code, 0x60);                             // pushad
		code = emitDword(code, 0x2424448D);                      // lea eax, [esp+0x24]
		code = emitByte(code, 0x50);                             // push eax
		code = emitRelative(code, 0xE8, reinterpret_cast<DWORD>(&onLeave));
		code = emitDword(code, 0x20244489);                      // mov [esp+0x20], eax
		code = emitByte(code, 0x61);                             // popad
		code = emitByte(code, 0xC3);                             // ret

		// Entry stubs: set the subject, run the displaced instructions, continue in the function.
		for (const auto& hook : entryHooks) {
			const auto stub = reinterpret_cast<DWORD>(code);
			code = emitByte(code, 0x60);                         // pushad
			code = emitByte(code, 0x54);                         // push esp
			code = emitByte(code, 0x68);                         // push kind
			code = emitDword(code, static_cast<DWORD>(hook.kind));
			code = emitRelative(code, 0xE8, reinterpret_cast<DWORD>(&onEnter));
			code = emitByte(code, 0x61);                         // popad
			memcpy(code, hook.expected, hook.length);
			code += hook.length;
			code = emitRelative(code, 0xE9, hook.address + hook.length);

			se::memory::genJumpUnprotected(hook.address, stub, hook.length);
		}

		for (const auto site : globalLevelCallSites) {
			se::memory::genCallEnforced(site, 0x51D760, reinterpret_cast<DWORD>(&getWaterMinLevel));
		}
		se::memory::genJumpUnprotected(0x53B4A0, reinterpret_cast<DWORD>(&isPointUnderwater));
		for (const auto site : cellLevelCallSites) {
			se::memory::genCallEnforced(site, 0x4E28B0, reinterpret_cast<DWORD>(&cellGetWaterLevel));
		}
		for (const auto site : underwaterStateCallSites) {
			se::memory::genCallEnforced(site, 0x440AF0, reinterpret_cast<DWORD>(&updateUnderwaterState));
		}
		for (const auto site : interiorCellLoadSites) {
			se::memory::genCallUnprotected(site, reinterpret_cast<DWORD>(&getInteriorCellOrProxy), sizeof(interiorCellLoadBytes));
		}

		installed = true;
		return true;
	}
}
