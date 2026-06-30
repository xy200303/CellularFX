#ifndef CELLULAR_AUTOMATA_WORLD_RENDERER_H
#define CELLULAR_AUTOMATA_WORLD_RENDERER_H

#include "core/isimulator.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace godot {

// Manages the cached image and Godot texture for a CASWorld.
//
// Responsibilities:
// - Keep a dirty flag so the simulator image is only pulled when needed.
// - Provide a duplicate of the cached image to external callers (so they can
//   resize/modify without corrupting the internal cache).
// - Reuse a single ImageTexture object and update its pixels each frame.
class WorldRenderer {
	Ref<ImageTexture> texture;
	Ref<Image> cached_image;
	bool dirty = true;

public:
	WorldRenderer() = default;

	void mark_dirty() { dirty = true; }
	bool is_dirty() const { return dirty; }
	bool has_texture() const { return texture.is_valid(); }

	// Pull the latest image from the simulator if the cache is dirty.
	void ensure_updated(ca::ISimulator &p_simulator) {
		if (!dirty) {
			return;
		}
		cached_image = p_simulator.render_image();
		dirty = false;
	}

	// Return a duplicate of the cached image for external use.
	Ref<Image> get_image_snapshot(ca::ISimulator &p_simulator) {
		ensure_updated(p_simulator);
		if (cached_image.is_null()) {
			return Ref<Image>();
		}
		return cached_image->duplicate();
	}

	// Return (creating or updating as needed) a single reusable ImageTexture.
	Ref<Texture2D> get_texture(ca::ISimulator &p_simulator) {
		ensure_updated(p_simulator);
		if (cached_image.is_null()) {
			return Ref<Texture2D>();
		}
		if (texture.is_null()) {
			texture = ImageTexture::create_from_image(cached_image);
		} else {
			texture->update(cached_image);
		}
		return texture;
	}

	void reset() {
		texture.unref();
		cached_image.unref();
		dirty = true;
	}
};

} // namespace godot

#endif // CELLULAR_AUTOMATA_WORLD_RENDERER_H
