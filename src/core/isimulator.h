#ifndef CELLULAR_AUTOMATA_ISIMULATOR_H
#define CELLULAR_AUTOMATA_ISIMULATOR_H

#include "core/material_registry.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace ca {

// Abstract simulation backend. Implementations may be CPU-based or GPU-based.
// The simulator owns the grid state and exposes a renderer that returns an
// internal Image cache. The owner (CASWorld) is responsible for duplicating
// that image before handing it to external callers.
class ISimulator {
public:
	virtual ~ISimulator() = default;

	// Initialize the simulator with the given grid size and shared material
	// registry. Returns false if initialization failed.
	virtual bool initialize(int p_width, int p_height, MaterialRegistry &p_registry) = 0;

	// Advance the simulation by one tick.
	virtual void update() = 0;

	// Grid mutation and query.
	virtual void set_cell(int p_x, int p_y, uint16_t p_material_id) = 0;
	virtual uint16_t get_cell(int p_x, int p_y) const = 0;

	// Return the current simulation frame as an Image. The returned image is
	// owned by the simulator and may be reused across frames; callers must
	// duplicate() it if they want to modify or own the data.
	virtual godot::Ref<godot::Image> render_image() = 0;

	virtual int get_particle_count() const = 0;
	virtual void clear() = 0;

	// Called when the shared MaterialRegistry has been modified after
	// initialization. GPU backends should refresh their uniform buffers.
	virtual void on_registry_changed() {}

	// Snapshot save/load. The format is backend-agnostic (material names + cells).
	virtual godot::PackedByteArray serialize() const = 0;
	virtual bool deserialize(const godot::PackedByteArray &p_data) = 0;
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_ISIMULATOR_H
