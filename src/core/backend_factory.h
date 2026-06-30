#ifndef CELLULAR_AUTOMATA_BACKEND_FACTORY_H
#define CELLULAR_AUTOMATA_BACKEND_FACTORY_H

#include "core/backend_policy.h"
#include "core/isimulator.h"

#include <godot_cpp/variant/string.hpp>
#include <memory>

namespace ca {

struct BackendCapabilities {
	bool available = false;
	godot::String reason; // Human-readable reason when unavailable.
};

class BackendFactory {
public:
	// Create a simulator for the requested backend. Returns nullptr if the
	// backend enum is invalid. The simulator is not initialized yet.
	static std::unique_ptr<ISimulator> create(int p_backend);

	// Check whether a backend is usable without actually initializing it.
	static BackendCapabilities query_capabilities(int p_backend);

	// Pick the effective backend for a grid of the given size under the policy.
	static int select_backend(int p_requested_backend, int p_cell_count, const BackendPolicy &p_policy);
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_BACKEND_FACTORY_H
