#pragma once

#include "NINode.h"

#include "NIBSAnimationNode.h"

namespace NI {
	struct BSAnimationManager : Node {
		TArray<Pointer<BSAnimationNode>> managedNodes; // 0xB0

		// The new object has a reference count of 0. The caller sets the first reference.
		static BSAnimationManager* create();

		static bool isExactType(const AVObject* object);

		void growWorldBoundFromChild(AVObject* child);
		void attachChildConservative(AVObject* child, bool useFirstAvailable = false);
		Pointer<AVObject> setChildAtConservative(unsigned int index, AVObject* child);
	};
	static_assert(sizeof(BSAnimationManager) == 0xC8, "NI::BSAnimationManager failed size validation");
}
