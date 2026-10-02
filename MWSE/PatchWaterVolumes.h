#pragma once

#include "NIPoint3.h"

namespace mwse::patch::waterVolumes {
	struct Volume {
		int id;
		NI::Point3 min;
		NI::Point3 max;
	};

	// Installs the hooks. Returns false and changes nothing if the executable does not match.
	bool install();
	bool isInstalled();

	// Adds a box of water. The surface is at max.z and the floor at min.z. Returns the volume id, or 0 on failure.
	int add(const NI::Point3& min, const NI::Point3& max);
	bool remove(int id);
	void clear();
	const std::vector<Volume>& getVolumes();

	// The surface height of the volume that contains the position, if any.
	std::optional<float> getSurfaceAt(const NI::Point3& position);
}
