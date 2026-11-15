#include "NIBSAnimationNode.h"

#include "ExceptionUtil.h"

namespace NI {
	void BSAnimationNode::ctor() {
#if defined(SE_NI_BSANIMATIONNODE_FNADDR_CTOR) && SE_NI_BSANIMATIONNODE_FNADDR_CTOR > 0
		reinterpret_cast<void(__thiscall*)(BSAnimationNode*)>(SE_NI_BSANIMATIONNODE_FNADDR_CTOR)(this);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<BSAnimationNode> BSAnimationNode::create() {
		return construct<BSAnimationNode>();
	}

	void BSParticleNode::ctor() {
#if defined(SE_NI_BSPARTICLENODE_VTBL) && SE_NI_BSPARTICLENODE_VTBL > 0
		BSAnimationNode::ctor();
		flags |= BSParticleNode::AVObjectFlag::Follow;
		vTable.asObject = reinterpret_cast<Object_vTable*>(SE_NI_BSPARTICLENODE_VTBL);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<BSParticleNode> BSParticleNode::create() {
		return construct<BSParticleNode>();
	}

	bool BSParticleNode::getFollowMode() const {
		return (flags & BSParticleNode::AVObjectFlag::Follow) != 0;
	}

	void BSParticleNode::setFollowMode(bool enable) {
		if (enable) {
			flags |= BSParticleNode::AVObjectFlag::Follow;
		}
		else {
			flags &= ~BSParticleNode::AVObjectFlag::Follow;
		}
	}
}

#if defined(SE_USE_LUA) && SE_USE_LUA == 1
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::BSAnimationNode)
MWSE_SOL_CUSTOMIZED_PUSHER_DEFINE_NI(NI::BSParticleNode)
#endif
