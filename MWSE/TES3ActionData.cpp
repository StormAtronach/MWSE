#include "TES3ActionData.h"

namespace TES3 {
	const auto TES3_ActionData_copy = reinterpret_cast<ActionData*(__thiscall*)(ActionData*, const ActionData*)>(0x52F690);
	ActionData* ActionData::copy(const ActionData* from) {
		// The nocked projectile belongs to the actor's current action data, never to a copy of it.
		const auto ownProjectile = nockedProjectile;
		TES3_ActionData_copy(this, from);
		nockedProjectile = ownProjectile;
		return this;
	}

	void ActionData::cleanupMobileActor(MobileActor* mobileActor) {
		if (mobileActor == nullptr) {
			return;
		}

		if (target == mobileActor) {
			target = nullptr;
		}
		if (hitTarget == mobileActor) {
			hitTarget = nullptr;
		}
	}

}
