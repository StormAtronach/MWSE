#pragma once

#include "NIAVObject.h"
#include "NIPoint3.h"

namespace mwse::patch::waterVolumes {
	struct FootprintTriangle {
		float ax, ay;
		float bx, by;
		float cx, cy;
	};

	struct Volume {
		int id;
		NI::Point3 min;
		NI::Point3 max;
		// Empty for a box. Otherwise the volume covers only the area under these triangles.
		std::vector<FootprintTriangle> footprint;
	};

	// Installs the hooks. Returns false and changes nothing if the executable does not match.
	bool install();
	bool isInstalled();

	// Adds a box of water. The surface is at max.z and the floor at min.z. Returns the volume id, or 0 on failure.
	int add(const NI::Point3& min, const NI::Point3& max);

	// Adds the water under the triangles of a scene graph branch, using their current world positions.
	// The surface is at the highest vertex and the floor is depth below it. Returns the volume id, or 0 on failure.
	int addFromNode(NI::AVObject* node, float depth);

	bool remove(int id);
	void clear();
	const std::vector<Volume>& getVolumes();

	// The surface height of the volume that contains the position, if any.
	std::optional<float> getSurfaceAt(const NI::Point3& position);
}
