#ifndef CELLULAR_AUTOMATA_BACKEND_POLICY_H
#define CELLULAR_AUTOMATA_BACKEND_POLICY_H

#include <godot_cpp/variant/string.hpp>

namespace ca {

// Configurable rules for choosing between CPU and GPU backends.
struct BackendPolicy {
	// If true, small grids automatically fall back to CPU to avoid GPU fixed
	// overhead (compute list, barriers, submit/sync) outweighing the benefit.
	bool allow_gpu_auto_fallback = true;

	// Grid cell count threshold below which the GPU backend is auto-fallback
	// to CPU when allow_gpu_auto_fallback is true.
	int gpu_fallback_cell_threshold = 64 * 64;

	// If true, never fall back regardless of grid size or capabilities.
	// Useful for testing the GPU path on small grids.
	bool force_gpu = false;

	static BackendPolicy defaults() { return BackendPolicy(); }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_BACKEND_POLICY_H
