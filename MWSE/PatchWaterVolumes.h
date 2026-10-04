#pragma once

#include "NIAVObject.h"
#include "NIPoint3.h"

namespace mwse::patch::waterVolumes {
	struct FootprintTriangle {
		NI::Point3 a;
		NI::Point3 b;
		NI::Point3 c;
		float denominator;
	};

	struct Volume {
		int id;
		// Bounds. For a box, max.z is the surface and min.z the floor.
		NI::Point3 min;
		NI::Point3 max;
		float depth;
		// The scene graph branch the triangles came from. Compared, never followed.
		const NI::AVObject* node;
		// Empty for a box. Otherwise the surface is these triangles, in world space. Where they lie
		// in one layer over a point the volume reaches depth below them; where they lie in several,
		// the lowest is the floor.
		std::vector<FootprintTriangle> footprint;
		// Triangle indices per grid cell over the bounds.
		std::vector<std::vector<unsigned int>> grid;
		unsigned int gridSize;
		float gridScaleX;
		float gridScaleY;
	};

	// Installs the hooks. Returns false and changes nothing if the executable does not match.
	bool install();
	bool isInstalled();

	// Adds a box of water. The surface is at max.z and the floor at min.z. Returns the volume id, or 0 on failure.
	int add(const NI::Point3& min, const NI::Point3& max);

	// Adds the water under the triangles of a scene graph branch, using their current world positions.
	// The triangles are the surface, which may slope, and the volume reaches depth below them.
	// Where triangles lie over one another, the lowest is the floor instead, and the water under
	// each of the others reaches down to the one below it.
	// Returns the volume id, or 0 on failure.
	int addFromNode(NI::AVObject* node, float depth);

	bool remove(int id);
	void clear();
	const std::vector<Volume>& getVolumes();

	// The surface height of the volume that contains the position, if any.
	std::optional<float> getSurfaceAt(const NI::Point3& position);
}
