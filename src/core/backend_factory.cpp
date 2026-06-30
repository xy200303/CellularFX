#include "backend_factory.h"

#include "cpu/cpu_simulator.h"
#include "gpu/gpu_simulator.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ca {

std::unique_ptr<ISimulator> BackendFactory::create(int p_backend) {
	switch (p_backend) {
		case 0: // CPU
			return std::make_unique<CPUSimulator>();
		case 1: // GPU
			return std::make_unique<GPUSimulator>();
		default:
			return nullptr;
	}
}

BackendCapabilities BackendFactory::query_capabilities(int p_backend) {
	BackendCapabilities caps;
	switch (p_backend) {
		case 0: // CPU
			caps.available = true;
			break;
		case 1: { // GPU
			godot::RenderingDevice *rd = godot::RenderingServer::get_singleton()->create_local_rendering_device();
			if (rd != nullptr) {
				caps.available = true;
				godot::memdelete(rd);
			} else {
				caps.available = false;
				caps.reason = "No local RenderingDevice available (headless or GPU-disabled process).";
			}
			break;
		}
		default:
			caps.available = false;
			caps.reason = "Unknown backend enum.";
			break;
	}
	return caps;
}

int BackendFactory::select_backend(int p_requested_backend, int p_cell_count, const BackendPolicy &p_policy) {
	if (p_requested_backend != 1) {
		return p_requested_backend;
	}

	if (p_policy.force_gpu) {
		return 1;
	}

	if (p_policy.allow_gpu_auto_fallback && p_cell_count <= p_policy.gpu_fallback_cell_threshold) {
		godot::UtilityFunctions::push_warning(
			"CASWorld: grid cell count (", p_cell_count,
			") is below GPU fallback threshold; using CPU backend. Set force_gpu=true to override.");
		return 0;
	}

	BackendCapabilities caps = query_capabilities(1);
	if (!caps.available) {
		godot::UtilityFunctions::push_warning("CASWorld: GPU backend unavailable (", caps.reason, "). Falling back to CPU.");
		return 0;
	}

	return 1;
}

} // namespace ca
