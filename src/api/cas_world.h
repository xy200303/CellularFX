#ifndef CAS_WORLD_H
#define CAS_WORLD_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "core/isimulator.h"
#include "cpu/cpu_simulator.h"
#include "gpu/gpu_simulator.h"

namespace godot {

class CASMaterial;

class CASWorld : public Node {
	GDCLASS(CASWorld, Node)

public:
	enum Backend {
		BACKEND_CPU = 0,
		BACKEND_GPU = 1
	};

private:
	int backend = BACKEND_CPU;
	int width = 512;
	int height = 512;
	bool initialized = false;

	ca::ISimulator *simulator = nullptr;
	Ref<ImageTexture> texture;
	mutable Ref<Image> cached_image;
	mutable bool image_dirty = true;

protected:
	static void _bind_methods();

public:
	CASWorld();
	~CASWorld();

	void set_backend(int p_backend);
	int get_backend() const;

	void set_world_size(int p_width, int p_height);
	Vector2i get_world_size() const;

	void init(int p_width, int p_height);
	void clear();
	void update();
	String get_backend_name() const;

	void register_material(const Ref<CASMaterial> &p_material);
	void set_cell(int p_x, int p_y, const String &p_material_name);
	String get_cell(int p_x, int p_y) const;

	Ref<Image> get_image() const;
	Ref<Texture2D> get_texture();
	int get_particle_count() const;

	Error save_world(const String &p_path) const;
	Error load_world(const String &p_path);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CASWorld::Backend)

#endif // CAS_WORLD_H
