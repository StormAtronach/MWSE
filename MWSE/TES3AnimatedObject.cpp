#include "TES3AnimatedObject.h"

#include "MemoryUtil.h"
#include "TES3Util.h"

#include "TES3Actor.h"
#include "TES3DataHandler.h"
#include "TES3Reference.h"

#include "NINode.h"

namespace TES3 {
	// The engine's functions that these members call. They keep their addresses and their conventions.
	const auto TES3_AnimatedObject_getSafeReferenceToThis = reinterpret_cast<void* (__thiscall*)(AnimatedObject*)>(0x4F10D0);
	const auto TES3_AnimatedObject_replaceVisualNode = reinterpret_cast<void(__thiscall*)(AnimatedObject*, NI::Node*)>(0x4F1230);
	const auto TES3_AnimatedObject_reloadBaseModelOfPhysicalObject = reinterpret_cast<bool(__thiscall*)(AnimatedObject*, const char*)>(0x4EFD70);
	const auto TES3_figureOutXMeshPrefix = reinterpret_cast<int(__cdecl*)(const char*, const char*, char*, bool)>(0x47D870);

	// The table of the records of the physical object and of the animated object.
	constexpr auto TES3_VIRTUAL_TABLE_ANIMATED_OBJECT = 0x74A580;
	constexpr auto TES3_VIRTUAL_TABLE_PHYSICAL_OBJECT = 0x74A2FC;
	constexpr auto TES3_VIRTUAL_TABLE_OFFSET_LOAD_ANIMATION = 0x148;

	// The engine's type tag of a container clone. MWSE's ObjectType has no name for it.
	constexpr unsigned int OBJECT_TYPE_CONTAINER_CLONE = 'CTNC';

	bool AnimatedObject::isCloneEntity() const {
		const auto type = static_cast<unsigned int>(objectType);
		if (type == OBJECT_TYPE_CONTAINER_CLONE || type == ObjectType::NPCClone || type == ObjectType::CreatureClone) {
			return true;
		}

		// The base records are clones when their base flag is clear.
		if (type == ObjectType::Creature || type == ObjectType::Container || type == ObjectType::NPC) {
			return reinterpret_cast<const Actor*>(this)->isClone();
		}

		return false;
	}

	bool AnimatedObject::loadAnimation() {
		const auto slot = reinterpret_cast<bool(__thiscall* const*)(AnimatedObject*)>(vTable.base) + TES3_VIRTUAL_TABLE_OFFSET_LOAD_ANIMATION / sizeof(void*);
		return (*slot)(this);
	}

	void AnimatedObject::releaseAnimKfData() {
		const auto meshData = DataHandler::get()->nonDynamicData->meshData;
		meshData->releaseKeyframes(animKfData);
		animKfData = nullptr;
	}

	void AnimatedObject::freeAnimData() {
		releaseAnimKfData();
	}

	bool AnimatedObject::loadModel() {
		const auto meshData = DataHandler::get()->nonDynamicData->meshData;
		char path[260];

		if (loadAnimation()) {
			TES3_figureOutXMeshPrefix(getModelPath(), "NIF", path, false);
		}
		else {
			snprintf(path, sizeof(path), "%s\\%s", "Meshes", getModelPath());
		}

		sceneNode = meshData->loadMesh(path);
		if (!sceneNode) {
			releaseAnimKfData();
			return false;
		}

		return true;
	}

	bool AnimatedObject::reloadBaseModel(const char* nodeName) {
		// The clones keep their model.
		if (isCloneEntity()) {
			return false;
		}

		releaseAnimKfData();
		return TES3_AnimatedObject_reloadBaseModelOfPhysicalObject(this, nodeName);
	}

	void AnimatedObject::releaseClone(int unknown) {
		if (!isCloneEntity() && referenceToThis) {
			referenceToThis = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(referenceToThis) - 1);
		}

		if (!TES3_AnimatedObject_getSafeReferenceToThis(this)) {
			// The model goes back to the cache when the scene node is a node.
			const auto rootNode = (sceneNode && sceneNode->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) ? sceneNode.get() : nullptr;
			DataHandler::get()->nonDynamicData->meshData->releaseMesh(rootNode);
			sceneNode = nullptr;
			sceneCollisionRoot = nullptr;
		}

		if (boundingBox && !sceneNode) {
			se::memory::_delete(boundingBox);
			boundingBox = nullptr;
		}

		// A clone, or an object that no reference uses, lets go of its keyframe file.
		if (isCloneEntity() || !referenceToThis) {
			releaseAnimKfData();
		}
	}

	void AnimatedObject::replaceVisualNode(NI::Node* newNode) {
		releaseAnimKfData();
		TES3_AnimatedObject_replaceVisualNode(this, newNode);
	}

	void AnimatedObject::dtor() {
		vTable.base = reinterpret_cast<BaseObjectVirtualTable*>(TES3_VIRTUAL_TABLE_ANIMATED_OBJECT);
		releaseAnimKfData();

		vTable.base = reinterpret_cast<BaseObjectVirtualTable*>(TES3_VIRTUAL_TABLE_PHYSICAL_OBJECT);
		mwse::tes3::setDataString(&objectID, nullptr);
		if (boundingBox) {
			se::memory::_delete(boundingBox);
			boundingBox = nullptr;
		}

		BaseObject::dtor();
	}
}
