#include "RenderSkipProbe.h"

#include "Log.h"
#include "MemoryUtil.h"
#include "MWSEConfig.h"

#include "NIAVObject.h"
#include "NICamera.h"
#include "NINode.h"
#include "NIProperty.h"
#include "NIRTTI.h"
#include "NITriBasedGeometry.h"
#include "NITriBasedGeometryData.h"

#include "TES3Cell.h"
#include "TES3DataHandler.h"
#include "TES3Object.h"
#include "TES3Reference.h"

namespace mwse::patch::renderskip {
	namespace {
		// HIDE_MASK lives in the header (shared with the static batching prototype).
		// No instruction in Morrowind.exe writes a value at or above 0x100 to
		// NiAVObject::flags (scanned 2026-09-14). 0x400 was the first choice and hid
		// every pick-proxy subtree until 09:50; the runs before that carry a constant
		// baseline offset at sites with pick-proxy meshes.

		// NiNodeFlags_10: NiNode::Display marks the node as displayed this frame;
		// ShadowManager::addObjectsAndLights and Reference::animIdleUpdate read it.
		constexpr unsigned short NODE_DISPLAYED_MASK = 0x0010;

		constexpr DWORD NiAVObject_CullShow = 0x6EB480;
		constexpr DWORD NiNode_Display = 0x6C9190;
		constexpr size_t IsVisualObject_vtableOffset = 0x64;

		constexpr unsigned int LOG_EVERY_FRAMES = 300;

		std::vector<NI::Pointer<NI::AVObject>> g_hidden;
		unsigned int g_eligibleRefs = 0;
		unsigned int g_eligibleShapes = 0;
		unsigned int g_eligibleTris = 0;
		unsigned int g_hiddenShapes = 0;
		unsigned int g_hiddenTris = 0;
		unsigned int g_partialRefs = 0;
		unsigned int g_rejectedShapes = 0;

		unsigned int g_skippedThisFrame = 0;
		unsigned int g_visitedThisFrame = 0;
		unsigned long long g_skippedAccum = 0;
		unsigned long long g_visitedAccum = 0;
		unsigned int g_framesAccum = 0;

		struct AppliedState {
			bool enabled = false;
			unsigned int percent = 0;
			unsigned int minTris = 0;
			unsigned int maxTris = 0;
			size_t cellSignature = 0;
		};
		AppliedState g_applied;

		bool isVisualObject(NI::AVObject* self) {
			const auto vtable = *reinterpret_cast<DWORD**>(self);
			const auto fn = reinterpret_cast<bool(__thiscall*)(NI::AVObject*)>(vtable[IsVisualObject_vtableOffset / sizeof(DWORD)]);
			return fn(self);
		}

		// Replaces NiNode::Display (0x6C9190) byte for byte in behaviour, except that
		// children carrying HIDE_MASK are not offered to CullShow. CullShow is called
		// through its engine address so an MSOC detour installed there still runs.
		void __fastcall NiNode_Display_detour(NI::Node* self, DWORD, NI::Camera* camera) {
			self->flags |= NODE_DISPLAYED_MASK;
			if (!isVisualObject(self)) {
				return;
			}
			const auto cullShow = reinterpret_cast<void(__thiscall*)(NI::AVObject*, NI::Camera*)>(NiAVObject_CullShow);
			auto& children = self->children;
			for (unsigned int i = 0; i < children.endIndex; ++i) {
				NI::AVObject* child = children.storage[i].get();
				if (!child) {
					continue;
				}
				++g_visitedThisFrame;
				if (child->flags & HIDE_MASK) {
					++g_skippedThisFrame;
					continue;
				}
				cullShow(child, camera);
			}
		}

		// ------------------------------------------------------------ eligibility

		struct WalkResult {
			std::vector<std::pair<NI::AVObject*, unsigned int>> shapes; // eligible shape, triangle count
			unsigned int rejectedShapes = 0;
		};

		// Node-level blockers reject the whole subtree. NiBSPNode and NiSwitchNode
		// (and NiLODNode under it) have their own Display that calls CullShow on
		// children directly, bypassing a hide bit on the child; the others animate
		// or re-orient; a non-light dynamic effect (NiTextureEffect) is state a
		// batch would inherit along with the lights.
		bool nodeBlocksSubtree(NI::AVObject* n) {
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiBillboardNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiSwitchNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiLODNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiBSPNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiBSAnimationNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiBSParticleNode)
				|| n->isInstanceOfType(NI::RTTIStaticPtr::NiParticles)) {
				return true;
			}
			if (n->controllers) {
				return true;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
				for (auto e = &static_cast<NI::Node*>(n)->effectList; e && e->data; e = e->next) {
					if (!e->data->isInstanceOfType(NI::RTTIStaticPtr::NiLight)) {
						return true;
					}
				}
			}
			return false;
		}

		// Shape-level blockers reject only this shape. Nearest property of each
		// type wins, as in NiGeometry::spPropertyState.
		bool shapeBlocked(NI::TriBasedGeometry* geom) {
			if (geom->skinInstance) {
				return true;
			}
			bool alphaResolved = false;
			for (auto cur = static_cast<NI::AVObject*>(geom); cur; cur = cur->parentNode) {
				for (auto node = &cur->propertyNode; node && node->data; node = node->next) {
					auto prop = node->data;
					if (prop->controllers) {
						return true; // animated texture / colour / alpha
					}
					if (!alphaResolved && prop->getType() == NI::PropertyType::Alpha) {
						alphaResolved = true;
						if (prop->flags & NI::AlphaProperty::ALPHA_MASK) {
							return true; // blended: sorted per object by the accumulator
						}
					}
				}
			}
			return false;
		}

		void walk(NI::AVObject* n, WalkResult& r, int depth, bool blocked) {
			if (n == nullptr || depth > 32) {
				return;
			}
			if (n->flags & 0x1) {
				return; // app-culled subtree (RootCollisionNode and friends): never a draw
			}
			if (!blocked && nodeBlocksSubtree(n)) {
				blocked = true;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiTriBasedGeom)) {
				auto geom = static_cast<NI::TriBasedGeometry*>(n);
				auto data = static_cast<NI::TriBasedGeometryData*>(geom->modelData.get());
				if (!data || data->vertexCount == 0) {
					return;
				}
				if (blocked || shapeBlocked(geom)) {
					++r.rejectedShapes;
					return;
				}
				r.shapes.emplace_back(n, data->triangleCount);
				return;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
				auto node = static_cast<NI::Node*>(n);
				for (unsigned int i = 0; i < node->children.endIndex; ++i) {
					walk(node->children.storage[i].get(), r, depth + 1, blocked);
				}
			}
		}

		// NiAVObject::LoadBinary reads the whole 16-bit flags word from the NIF and
		// CopyMembers copies it on clone, so the bit can arrive set on shapes this
		// probe never chose. Clear it on every shape under every reference before
		// choosing, so a stray bit cannot hide geometry outside the sample.
		void clearBitsUnder(NI::AVObject* n, int depth) {
			if (n == nullptr || depth > 32) {
				return;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiTriBasedGeom)) {
				n->flags &= ~HIDE_MASK;
				return;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
				auto node = static_cast<NI::Node*>(n);
				for (unsigned int i = 0; i < node->children.endIndex; ++i) {
					clearBitsUnder(node->children.storage[i].get(), depth + 1);
				}
			}
		}

		// FNV-1a over the object id and position: stable across sessions for the
		// same reference, so a sweep percentage always hides the same set.
		unsigned int referenceHash(TES3::Reference* ref) {
			unsigned int h = 2166136261u;
			auto mix = [&h](unsigned char b) { h ^= b; h *= 16777619u; };
			if (auto id = ref->baseObject ? ref->baseObject->getObjectID() : nullptr) {
				for (const char* p = id; *p; ++p) mix(static_cast<unsigned char>(*p));
			}
			if (auto pos = ref->getPosition()) {
				const auto bytes = reinterpret_cast<const unsigned char*>(pos);
				for (size_t i = 0; i < sizeof(NI::Point3); ++i) mix(bytes[i]);
			}
			return h;
		}

		void clearHidden() {
			for (auto& shape : g_hidden) {
				if (shape) {
					shape->flags &= ~HIDE_MASK;
				}
			}
			g_hidden.clear();
			g_hiddenShapes = 0;
			g_hiddenTris = 0;
			g_eligibleRefs = 0;
			g_partialRefs = 0;
			g_eligibleShapes = 0;
			g_rejectedShapes = 0;
			g_eligibleTris = 0;
		}

		template <typename Fn>
		void forEachActiveReferenceList(TES3::DataHandler* dataHandler, Fn&& fn) {
			auto visitCell = [&fn](TES3::Cell* cell) {
				fn(cell->actors);
				fn(cell->persistentRefs);
				fn(cell->temporaryRefs);
			};
			if (dataHandler->currentInteriorCell) {
				visitCell(dataHandler->currentInteriorCell);
				return;
			}
			for (auto entry : dataHandler->exteriorCellData) {
				if (!entry || entry->state != TES3::ExteriorDataLoadingState::Loaded || !entry->cell) {
					continue;
				}
				visitCell(entry->cell);
			}
		}

		void considerReferenceList(TES3::ReferenceList& list, const AppliedState& cfg) {
			for (auto ref : list) {
				if (!ref || !ref->baseObject || ref->baseObject->objectType != TES3::ObjectType::Static) {
					continue;
				}
				auto node = ref->sceneNode.get();
				if (!node || ref->getDeleted()) {
					continue;
				}
				WalkResult result;
				walk(node, result, 0, false);
				g_rejectedShapes += result.rejectedShapes;
				if (result.shapes.empty()) {
					continue;
				}
				++g_eligibleRefs;
				if (result.rejectedShapes > 0) {
					++g_partialRefs;
				}
				const bool hideThisRef = (referenceHash(ref) % 100u) < cfg.percent;
				for (const auto& [shape, tris] : result.shapes) {
					++g_eligibleShapes;
					g_eligibleTris += tris;
					if (!hideThisRef || tris < cfg.minTris || tris >= cfg.maxTris) {
						continue;
					}
					shape->flags |= HIDE_MASK;
					g_hidden.emplace_back(shape);
					++g_hiddenShapes;
					g_hiddenTris += tris;
				}
			}
		}

		size_t activeCellSignature(TES3::DataHandler* dataHandler) {
			size_t sig = reinterpret_cast<size_t>(dataHandler->currentInteriorCell);
			for (auto entry : dataHandler->exteriorCellData) {
				if (!entry) continue;
				sig = sig * 31 + reinterpret_cast<size_t>(entry->cell);
				sig = sig * 31 + static_cast<size_t>(entry->state);
			}
			return sig;
		}

		void rebuild(TES3::DataHandler* dataHandler, const AppliedState& cfg) {
			clearHidden();
			// Hygiene pass over every reference of every type, so a NIF-loaded or
			// cloned copy of the bit cannot survive outside the sample.
			forEachActiveReferenceList(dataHandler, [](TES3::ReferenceList& list) {
				for (auto ref : list) {
					if (ref && ref->sceneNode) {
						clearBitsUnder(ref->sceneNode.get(), 0);
					}
				}
			});
			if (!cfg.enabled) {
				return;
			}
			forEachActiveReferenceList(dataHandler, [&cfg](TES3::ReferenceList& list) {
				considerReferenceList(list, cfg);
			});
			log::getLog() << "[RenderSkip] rebuild percent=" << cfg.percent
				<< " tris=[" << cfg.minTris << "," << cfg.maxTris << ")"
				<< " eligibleRefs=" << g_eligibleRefs
				<< " partialRefs=" << g_partialRefs
				<< " eligibleShapes=" << g_eligibleShapes
				<< " rejectedShapes=" << g_rejectedShapes
				<< " eligibleTris=" << g_eligibleTris
				<< " hiddenShapes=" << g_hiddenShapes
				<< " hiddenTris=" << g_hiddenTris
				<< std::endl;
		}
	}

	void install() {
		se::memory::genJumpUnprotected(NiNode_Display, reinterpret_cast<DWORD>(NiNode_Display_detour));
		g_hidden.reserve(4096);
	}

	void onFrame() {
		// Fold the previous frame's counters into the window before anything else.
		g_skippedAccum += g_skippedThisFrame;
		g_visitedAccum += g_visitedThisFrame;
		g_skippedThisFrame = 0;
		g_visitedThisFrame = 0;

		auto dataHandler = TES3::DataHandler::get();
		if (!dataHandler) {
			return;
		}

		AppliedState wanted;
		// The batching prototype owns the hide bit while it is on; the probe must
		// neither hide nor clear anything then.
		wanted.enabled = Configuration::EnableRenderSkipProbe && !Configuration::EnableStaticBatching;
		wanted.percent = Configuration::RenderSkipPercent > 100u ? 100u : Configuration::RenderSkipPercent;
		wanted.minTris = Configuration::RenderSkipMinTriangles;
		wanted.maxTris = Configuration::RenderSkipMaxTriangles;
		wanted.cellSignature = activeCellSignature(dataHandler);

		const bool changed = wanted.enabled != g_applied.enabled
			|| wanted.percent != g_applied.percent
			|| wanted.minTris != g_applied.minTris
			|| wanted.maxTris != g_applied.maxTris
			|| wanted.cellSignature != g_applied.cellSignature;
		if (changed) {
			// Only rebuild while enabled or when turning off; an idle probe costs nothing.
			if (wanted.enabled || g_applied.enabled) {
				rebuild(dataHandler, wanted);
			}
			g_applied = wanted;
			g_skippedAccum = 0;
			g_visitedAccum = 0;
			g_framesAccum = 0;
		}

		if (!g_applied.enabled) {
			return;
		}
		if (++g_framesAccum >= LOG_EVERY_FRAMES) {
			log::getLog() << "[RenderSkip] window frames=" << g_framesAccum
				<< " percent=" << g_applied.percent
				<< " tris=[" << g_applied.minTris << "," << g_applied.maxTris << ")"
				<< " hiddenShapes=" << g_hiddenShapes
				<< " skipped/frame=" << (g_skippedAccum / g_framesAccum)
				<< " visited/frame=" << (g_visitedAccum / g_framesAccum)
				<< std::endl;
			g_skippedAccum = 0;
			g_visitedAccum = 0;
			g_framesAccum = 0;
		}
	}
}
