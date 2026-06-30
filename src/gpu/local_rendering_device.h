#ifndef CELLULAR_AUTOMATA_LOCAL_RENDERING_DEVICE_H
#define CELLULAR_AUTOMATA_LOCAL_RENDERING_DEVICE_H

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/core/memory.hpp>

namespace ca {

// RAII wrapper around a locally-created Godot RenderingDevice.
// Guarantees that memdelete(rd) is called exactly once when the wrapper is
// destroyed or reset, preventing ObjectDB leaks.
class LocalRenderingDevice {
	godot::RenderingDevice *rd = nullptr;

public:
	LocalRenderingDevice() = default;
	~LocalRenderingDevice() { reset(nullptr); }

	// Non-copyable.
	LocalRenderingDevice(const LocalRenderingDevice &) = delete;
	LocalRenderingDevice &operator=(const LocalRenderingDevice &) = delete;

	// Movable.
	LocalRenderingDevice(LocalRenderingDevice &&p_other) noexcept : rd(p_other.rd) {
		p_other.rd = nullptr;
	}
	LocalRenderingDevice &operator=(LocalRenderingDevice &&p_other) noexcept {
		if (this != &p_other) {
			reset(p_other.rd);
			p_other.rd = nullptr;
		}
		return *this;
	}

	void reset(godot::RenderingDevice *p_rd = nullptr) {
		if (rd != nullptr && rd != p_rd) {
			godot::memdelete(rd);
		}
		rd = p_rd;
	}

	godot::RenderingDevice *get() const { return rd; }
	godot::RenderingDevice *operator->() const { return rd; }
	operator godot::RenderingDevice *() const { return rd; }
	bool is_valid() const { return rd != nullptr; }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_LOCAL_RENDERING_DEVICE_H
