#pragma once

#include "TES3Defines.h"

#include "TES3Object.h"

namespace TES3 {
	// The engine's EntityAnimated: a physical object that owns a keyframe file (animKfData). The records that animate (NPCs,
	// creatures, containers, doors, activators, lights) are built on it.
	//
	// The members below take the place of six engine functions. Each held an inline copy of the release of animKfData that
	// changed the keyframe map and the use count of the file with no lock. They run it through MeshData::releaseKeyframes,
	// which takes the keyframe lock. Every other step is the engine's.
	struct AnimatedObject : PhysicalObject {
		KeyframeDefinition* animKfData; // 0x30

		//
		// The engine's functions, which a jump at their entries sends here. The engine reaches loadModel, reloadBaseModel,
		// releaseClone and replaceVisualNode through the virtual tables of its records.
		//

		bool loadModel(); // 0x4F0400
		bool reloadBaseModel(const char* nodeName); // 0x4F0600
		void releaseClone(int unknown); // 0x4F0890
		void replaceVisualNode(NI::Node* newNode); // 0x4F0A90
		void freeAnimData(); // 0x4F02C0
		void dtor(); // 0x4F0140

		//
		// Helpers.
		//

		// Releases animKfData through the keyframe map and clears it.
		void releaseAnimKfData();
		// The engine's Entity::isClone (0x4F0FF0).
		bool isCloneEntity() const;
		// The engine's virtual loadAnimation (table offset 0x148).
		bool loadAnimation();
	};
	static_assert(sizeof(AnimatedObject) == 0x34, "TES3::AnimatedObject failed size validation");
}
