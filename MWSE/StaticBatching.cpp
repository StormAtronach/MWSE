#include "StaticBatching.h"

#include "Log.h"
#include "MWSEConfig.h"
#include "RenderSkipProbe.h"

#include "NIAVObject.h"
#include "NIBound.h"
#include "NIDynamicEffect.h"
#include "NILight.h"
#include "NIMatrix33.h"
#include "NINode.h"
#include "NIPointer.h"
#include "NIProperty.h"
#include "NIRTTI.h"
#include "NITriangle.h"
#include "NITriBasedGeometry.h"
#include "NITriShape.h"
#include "NITriShapeData.h"

#include "TES3Cell.h"
#include "TES3DataHandler.h"
#include "TES3Object.h"
#include "TES3Reference.h"
#include "TES3WorldController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <vector>

namespace mwse::patch::staticbatching {
	namespace {
		using renderskip::HIDE_MASK;

		constexpr unsigned int VERTEX_LIMIT = 65535;
		constexpr int PROPERTY_SLOTS = 16;

		struct Batch {
			NI::Node* node = nullptr;   // owned by g_batchRoot through the child list
			unsigned int members = 0;
			unsigned int verts = 0;
			unsigned int tris = 0;
			unsigned int lights = 0;
			bool overCap = false;
		};

		// Heap-allocated and never freed on purpose: NI::Pointer releases at DLL
		// detach run after the engine has torn the scene down (see the MSOC exit
		// crash note). Rebuilds release everything explicitly while the game runs.
		std::vector<Batch>* g_batches = new std::vector<Batch>();
		std::vector<NI::Pointer<NI::AVObject>>* g_hidden = new std::vector<NI::Pointer<NI::AVObject>>();
		NI::Node* g_batchRoot = nullptr;
		bool g_loggedRoot = false;

		struct AppliedState {
			bool enabled = false;
			unsigned int binSize = 0;
			unsigned int types = 0;
			unsigned int maxLights = 0;
			size_t cellSignature = 0;
		};
		AppliedState g_applied;

		// ------------------------------------------------------------ eligibility
		// Same rules as the render-skip probe and the census (plan section 4.4).

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

		struct Resolved {
			NI::Property* props[PROPERTY_SLOTS] = {};
		};

		// Nearest property of each type wins, as in NiGeometry::spPropertyState.
		void resolveProperties(NI::AVObject* shape, Resolved& out) {
			for (auto cur = shape; cur; cur = cur->parentNode) {
				for (auto node = &cur->propertyNode; node && node->data; node = node->next) {
					const int t = static_cast<int>(node->data->getType());
					if (t >= 0 && t < PROPERTY_SLOTS && !out.props[t]) {
						out.props[t] = node->data;
					}
				}
			}
		}

		bool shapeBlocked(NI::TriBasedGeometry* geom, const Resolved& r) {
			if (geom->skinInstance) {
				return true;
			}
			for (int t = 0; t < PROPERTY_SLOTS; ++t) {
				if (r.props[t] && r.props[t]->controllers) {
					return true; // animated texture / colour / alpha
				}
			}
			auto alpha = r.props[static_cast<int>(NI::PropertyType::Alpha)];
			if (alpha && (alpha->flags & NI::AlphaProperty::ALPHA_MASK)) {
				return true; // blended: sorted per object by the accumulator
			}
			return false;
		}

		// ------------------------------------------------------------ signature

		struct Hasher {
			unsigned long long h = 1469598103934665603ull;
			void bytes(const void* p, size_t n) {
				auto b = static_cast<const unsigned char*>(p);
				for (size_t i = 0; i < n; ++i) {
					h ^= b[i];
					h *= 1099511628211ull;
				}
			}
			template <typename T>
			void v(const T& x) { bytes(&x, sizeof(T)); }
		};

		// Everything NiDX8Renderer reads to set up one draw, hashed by value, plus
		// the vertex layout. Two shapes with equal signatures can share a batch.
		unsigned long long signatureOf(const Resolved& r, NI::GeometryData* data) {
			Hasher h;
			for (int t = 0; t < PROPERTY_SLOTS; ++t) {
				auto p = r.props[t];
				h.v(t);
				if (!p) {
					h.v(0);
					continue;
				}
				h.v(p->flags);
				switch (static_cast<NI::PropertyType>(t)) {
				case NI::PropertyType::Alpha:
					h.v(static_cast<NI::AlphaProperty*>(p)->alphaTestRef);
					break;
				case NI::PropertyType::Fog: {
					auto f = static_cast<NI::FogProperty*>(p);
					h.v(f->density);
					h.bytes(f->color, sizeof(f->color));
					break;
				}
				case NI::PropertyType::Material: {
					auto m = static_cast<NI::MaterialProperty*>(p);
					h.v(m->ambient);
					h.v(m->diffuse);
					h.v(m->specular);
					h.v(m->emissive);
					h.v(m->shininess);
					h.v(m->alpha);
					break;
				}
				case NI::PropertyType::Stencil:
					// All plain integers past the Property header.
					h.bytes(reinterpret_cast<const unsigned char*>(p) + sizeof(NI::Property), sizeof(NI::StencilProperty) - sizeof(NI::Property));
					break;
				case NI::PropertyType::Texturing: {
					auto tp = static_cast<NI::TexturingProperty*>(p);
					h.v(tp->applyMode);
					for (unsigned int i = 0; i < tp->maps.endIndex; ++i) {
						auto m = tp->maps.storage[i];
						if (!m) {
							h.v(0);
							continue;
						}
						h.v(m->texture.get());
						h.v(m->clampMode);
						h.v(m->filterMode);
						h.v(m->texCoordSet);
					}
					break;
				}
				case NI::PropertyType::VertexColor: {
					auto vc = static_cast<NI::VertexColorProperty*>(p);
					h.v(vc->source);
					h.v(vc->lighting);
					break;
				}
				case NI::PropertyType::ZBuffer:
					h.v(static_cast<NI::ZBufferProperty*>(p)->testFunction);
					break;
				case NI::PropertyType::RendererSpecific:
					h.v(p);
					break;
				default:
					break; // wireframe, dither, specular, shade: flags only
				}
			}
			h.v(data->normal != nullptr);
			h.v(data->color != nullptr);
			h.v(data->textureSets);
			return h.h;
		}

		// ------------------------------------------------------------ collection

		struct Member {
			NI::TriBasedGeometry* shape;
			NI::TriShapeData* data;
			TES3::Reference* ref;
		};

		struct Group {
			std::vector<Member> members;
			Resolved proto;
			bool hasNormals = false;
			bool hasColors = false;
			unsigned short textureSets = 0;
		};

		struct GroupKey {
			unsigned long long sig;
			int bx;
			int by;
			bool operator<(const GroupKey& o) const {
				if (sig != o.sig) return sig < o.sig;
				if (bx != o.bx) return bx < o.bx;
				return by < o.by;
			}
		};

		using GroupMap = std::map<GroupKey, Group>;

		void collectShapes(NI::AVObject* n, TES3::Reference* ref, GroupMap& groups, const AppliedState& cfg, bool blocked, int depth, unsigned int& rejected) {
			if (n == nullptr || depth > 32) {
				return;
			}
			if (n->flags & 0x1) {
				return; // app-culled subtree
			}
			if (!blocked && nodeBlocksSubtree(n)) {
				blocked = true;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiTriBasedGeom)) {
				auto geom = static_cast<NI::TriBasedGeometry*>(n);
				auto base = geom->modelData.get();
				if (!base || base->vertexCount == 0) {
					return;
				}
				// Triangle lists only; strips would need conversion.
				if (!base->isInstanceOfType(NI::RTTIStaticPtr::NiTriShapeData)) {
					++rejected;
					return;
				}
				auto data = static_cast<NI::TriShapeData*>(base);
				if (blocked || data->triangleCount == 0 || data->textureSets > 1) {
					++rejected;
					return;
				}
				Resolved r;
				resolveProperties(n, r);
				if (shapeBlocked(geom, r)) {
					++rejected;
					return;
				}
				GroupKey key;
				key.sig = signatureOf(r, data);
				key.bx = static_cast<int>(std::floor(n->worldBoundOrigin.x / static_cast<float>(cfg.binSize)));
				key.by = static_cast<int>(std::floor(n->worldBoundOrigin.y / static_cast<float>(cfg.binSize)));
				auto& g = groups[key];
				if (g.members.empty()) {
					g.proto = r;
					g.hasNormals = data->normal != nullptr;
					g.hasColors = data->color != nullptr;
					g.textureSets = data->textureSets;
				}
				g.members.push_back({ geom, data, ref });
				return;
			}
			if (n->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
				auto node = static_cast<NI::Node*>(n);
				for (unsigned int i = 0; i < node->children.endIndex; ++i) {
					collectShapes(node->children.storage[i].get(), ref, groups, cfg, blocked, depth + 1, rejected);
				}
			}
		}

		bool wantedType(TES3::ObjectType::ObjectType type, unsigned int mask) {
			switch (type) {
			case TES3::ObjectType::Static: return (mask & 1) != 0;
			case TES3::ObjectType::Activator: return (mask & 2) != 0;
			case TES3::ObjectType::Container: return (mask & 4) != 0;
			case TES3::ObjectType::Door: return (mask & 8) != 0;
			default: return false;
			}
		}

		template <typename Fn>
		void forEachActiveCell(TES3::DataHandler* dataHandler, Fn&& fn) {
			if (dataHandler->currentInteriorCell) {
				fn(dataHandler->currentInteriorCell);
				return;
			}
			for (auto entry : dataHandler->exteriorCellData) {
				if (entry && entry->state == TES3::ExteriorDataLoadingState::Loaded && entry->cell) {
					fn(entry->cell);
				}
			}
		}

		void collectReferences(TES3::ReferenceList& list, GroupMap& groups, const AppliedState& cfg, unsigned int& refs, unsigned int& rejected) {
			for (auto ref : list) {
				if (!ref || !ref->baseObject || !wantedType(ref->baseObject->objectType, cfg.types)) {
					continue;
				}
				auto node = ref->sceneNode.get();
				if (!node || ref->getDeleted()) {
					continue;
				}
				++refs;
				collectShapes(node, ref, groups, cfg, false, 0, rejected);
			}
		}

		// ------------------------------------------------------------ scene root

		NI::Node* ensureBatchRoot() {
			auto worldController = TES3::WorldController::get();
			if (!worldController) {
				return nullptr;
			}
			auto sceneRoot = worldController->worldCamera.root.get();
			if (!sceneRoot) {
				return nullptr;
			}
			auto worldRoot = static_cast<NI::Node*>(sceneRoot->getObjectByName("worldRoot"));
			if (!worldRoot || !worldRoot->isInstanceOfType(NI::RTTIStaticPtr::NiNode)) {
				worldRoot = sceneRoot;
			}
			if (g_batchRoot && g_batchRoot->parentNode != worldRoot) {
				// The engine rebuilt the scene under us; drop our stale handle. The
				// old root is owned by whoever still holds it, or already gone.
				g_batchRoot = nullptr;
			}
			if (!g_batchRoot) {
				auto root = NI::Node::create();
				root->setName("MWSE_BatchRoot");
				worldRoot->attachChild(root, true);
				g_batchRoot = root.get();
				// Pin it: the raw pointer above must outlive any engine child churn.
				++g_batchRoot->refCount;
				if (!g_loggedRoot) {
					g_loggedRoot = true;
					const auto& wt = worldRoot->worldTransform;
					log::getLog() << "[StaticBatching] batch root under '" << (worldRoot->name ? worldRoot->name : "?")
						<< "' world translation (" << wt.translation.x << ", " << wt.translation.y << ", " << wt.translation.z
						<< ") scale " << wt.scale << std::endl;
				}
			}
			return g_batchRoot;
		}

		// ------------------------------------------------------------ build / teardown

		void teardown() {
			for (auto& b : *g_batches) {
				if (b.node) {
					b.node->detachAllEffects();
					if (g_batchRoot && b.node->parentNode == g_batchRoot) {
						g_batchRoot->detachChild(b.node); // returned Pointer releases the node
					}
				}
			}
			g_batches->clear();
			for (auto& shape : *g_hidden) {
				if (shape) {
					shape->flags &= ~HIDE_MASK;
				}
			}
			g_hidden->clear();
		}

		struct LightRef {
			NI::DynamicEffect* light;
			float distance;
		};

		void gatherLights(const std::vector<Member>& members, const NI::Point3& center, std::vector<LightRef>& out) {
			for (const auto& m : members) {
				auto node = m.ref->sceneNode.get();
				if (!node) {
					continue;
				}
				for (auto e = &node->effectList; e && e->data; e = e->next) {
					auto effect = e->data;
					if (!effect->isInstanceOfType(NI::RTTIStaticPtr::NiLight)) {
						continue;
					}
					bool seen = false;
					for (const auto& l : out) {
						if (l.light == effect) {
							seen = true;
							break;
						}
					}
					if (seen) {
						continue;
					}
					const auto& p = effect->worldTransform.translation;
					const float dx = p.x - center.x, dy = p.y - center.y, dz = p.z - center.z;
					out.push_back({ effect, std::sqrt(dx * dx + dy * dy + dz * dz) });
				}
			}
			std::sort(out.begin(), out.end(), [](const LightRef& a, const LightRef& b) { return a.distance < b.distance; });
		}

		void buildBatch(const Group& g, size_t first, size_t last, const AppliedState& cfg) {
			unsigned int verts = 0, tris = 0;
			NI::Point3 center(0.0f, 0.0f, 0.0f);
			for (size_t i = first; i < last; ++i) {
				verts += g.members[i].data->vertexCount;
				tris += g.members[i].data->triangleCount;
				center = center + g.members[i].shape->worldBoundOrigin;
			}
			const float inv = 1.0f / static_cast<float>(last - first);
			center = center * inv;

			auto shape = NI::TriShape::create(static_cast<unsigned short>(verts), g.hasNormals, g.hasColors, g.textureSets, static_cast<unsigned short>(tris));
			auto data = shape->getModelData();
			unsigned int vbase = 0, tbase = 0;
			for (size_t i = first; i < last; ++i) {
				const auto& m = g.members[i];
				const auto& wt = m.shape->worldTransform;
				const auto src = m.data;
				for (unsigned int v = 0; v < src->vertexCount; ++v) {
					const NI::Point3 world = wt.rotation * (src->vertex[v] * wt.scale) + wt.translation;
					data->vertex[vbase + v] = world - center;
					if (g.hasNormals) {
						data->normal[vbase + v] = src->normal ? (wt.rotation * src->normal[v]) : NI::Point3(0.0f, 0.0f, 1.0f);
					}
					if (g.hasColors) {
						if (src->color) {
							data->color[vbase + v] = src->color[v];
						}
						else {
							auto& c = data->color[vbase + v];
							c.r = c.g = c.b = c.a = 255;
						}
					}
					if (g.textureSets > 0) {
						data->textureCoords[vbase + v] = src->textureCoords ? src->textureCoords[v] : NI::Point2(0.0f, 0.0f);
					}
				}
				for (unsigned int t = 0; t < src->triangleCount; ++t) {
					const auto& s = src->triangleList[t];
					auto& d = data->triangleList[tbase + t];
					d.vertices[0] = static_cast<unsigned short>(vbase + s.vertices[0]);
					d.vertices[1] = static_cast<unsigned short>(vbase + s.vertices[1]);
					d.vertices[2] = static_cast<unsigned short>(vbase + s.vertices[2]);
				}
				vbase += src->vertexCount;
				tbase += src->triangleCount;
			}
			data->bounds.computeFromData(verts, data->vertex, sizeof(NI::Point3));
			data->markAsChanged();

			auto node = NI::Node::create();
			node->setName("MWSE_Batch");
			node->localTranslate = center;
			node->attachChild(shape, true);
			for (int t = 0; t < PROPERTY_SLOTS; ++t) {
				if (g.proto.props[t]) {
					shape->attachProperty(g.proto.props[t]);
				}
			}
			g_batchRoot->attachChild(node, true);

			std::vector<Member> chunk(g.members.begin() + first, g.members.begin() + last);
			std::vector<LightRef> lights;
			gatherLights(chunk, center, lights);
			unsigned int attached = 0;
			for (const auto& l : lights) {
				if (attached >= cfg.maxLights) {
					break;
				}
				node->attachEffect(l.light);
				++attached;
			}

			for (const auto& m : chunk) {
				m.shape->flags |= HIDE_MASK;
				g_hidden->emplace_back(m.shape);
			}

			Batch b;
			b.node = node.get();
			b.members = static_cast<unsigned int>(last - first);
			b.verts = verts;
			b.tris = tris;
			b.lights = attached;
			b.overCap = lights.size() > cfg.maxLights;
			g_batches->push_back(b);
		}

		void rebuild(TES3::DataHandler* dataHandler, const AppliedState& cfg) {
			const auto t0 = std::chrono::steady_clock::now();
			teardown();
			if (!cfg.enabled) {
				return;
			}
			if (!ensureBatchRoot()) {
				log::getLog() << "[StaticBatching] no world root; batching idle" << std::endl;
				return;
			}

			GroupMap groups;
			unsigned int refs = 0, rejected = 0;
			forEachActiveCell(dataHandler, [&](TES3::Cell* cell) {
				collectReferences(cell->persistentRefs, groups, cfg, refs, rejected);
				collectReferences(cell->temporaryRefs, groups, cfg, refs, rejected);
			});

			unsigned int members = 0, verts = 0, tris = 0, lights = 0, overCap = 0;
			for (auto& [key, g] : groups) {
				// Split at the 16-bit vertex limit, in member order.
				size_t first = 0;
				unsigned int acc = 0;
				for (size_t i = 0; i < g.members.size(); ++i) {
					const unsigned int mv = g.members[i].data->vertexCount;
					if (mv > VERTEX_LIMIT) {
						continue; // cannot fit even alone; leave it drawn as is
					}
					if (acc + mv > VERTEX_LIMIT) {
						buildBatch(g, first, i, cfg);
						first = i;
						acc = 0;
					}
					acc += mv;
				}
				if (first < g.members.size()) {
					buildBatch(g, first, g.members.size(), cfg);
				}
			}
			for (const auto& b : *g_batches) {
				members += b.members;
				verts += b.verts;
				tris += b.tris;
				lights += b.lights;
				if (b.overCap) ++overCap;
			}

			g_batchRoot->update(0.0f, false, true);
			g_batchRoot->updateProperties();
			g_batchRoot->updateEffects();

			const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			log::getLog() << "[StaticBatching] build ms=" << ms
				<< " bin=" << cfg.binSize << " types=" << cfg.types
				<< " refs=" << refs << " groups=" << groups.size()
				<< " batches=" << g_batches->size() << " members=" << members
				<< " rejectedShapes=" << rejected
				<< " verts=" << verts << " tris=" << tris
				<< " lightsAttached=" << lights << " batchesOverCap=" << overCap
				<< " hidden=" << g_hidden->size()
				<< std::endl;
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
	}

	void onFrame() {
		auto dataHandler = TES3::DataHandler::get();
		if (!dataHandler) {
			return;
		}
		AppliedState wanted;
		wanted.enabled = Configuration::EnableStaticBatching;
		wanted.binSize = Configuration::StaticBatchingBinSize < 256u ? 256u : Configuration::StaticBatchingBinSize;
		wanted.types = Configuration::StaticBatchingTypes;
		wanted.maxLights = Configuration::StaticBatchingMaxLights;
		wanted.cellSignature = activeCellSignature(dataHandler);

		const bool changed = wanted.enabled != g_applied.enabled
			|| wanted.binSize != g_applied.binSize
			|| wanted.types != g_applied.types
			|| wanted.maxLights != g_applied.maxLights
			|| wanted.cellSignature != g_applied.cellSignature;
		if (!changed) {
			return;
		}
		if (wanted.enabled || g_applied.enabled) {
			rebuild(dataHandler, wanted);
		}
		g_applied = wanted;
	}
}
