#include "cas_world.h"
#include "cas_material.h"

#include "core/backend_factory.h"

#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

CASWorld::CASWorld() {
}

CASWorld::~CASWorld() {
	// unique_ptr destructor calls ISimulator::~ISimulator, which releases
	// all backend resources (RIDs, local RenderingDevice, thread pool, etc.).
}

void CASWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_backend", "backend"), &CASWorld::set_backend);
	ClassDB::bind_method(D_METHOD("get_backend"), &CASWorld::get_backend);

	ClassDB::bind_method(D_METHOD("set_force_gpu", "force"), &CASWorld::set_force_gpu);
	ClassDB::bind_method(D_METHOD("get_force_gpu"), &CASWorld::get_force_gpu);

	ClassDB::bind_method(D_METHOD("set_gpu_fallback_threshold", "threshold"), &CASWorld::set_gpu_fallback_threshold);
	ClassDB::bind_method(D_METHOD("get_gpu_fallback_threshold"), &CASWorld::get_gpu_fallback_threshold);

	ClassDB::bind_method(D_METHOD("set_world_size", "width", "height"), &CASWorld::set_world_size);
	ClassDB::bind_method(D_METHOD("get_world_size"), &CASWorld::get_world_size);

	ClassDB::bind_method(D_METHOD("init", "width", "height"), &CASWorld::init);
	ClassDB::bind_method(D_METHOD("clear"), &CASWorld::clear);
	ClassDB::bind_method(D_METHOD("update"), &CASWorld::update);
	ClassDB::bind_method(D_METHOD("get_backend_name"), &CASWorld::get_backend_name);

	ClassDB::bind_method(D_METHOD("register_material", "material"), &CASWorld::register_material);
	ClassDB::bind_method(D_METHOD("set_cell", "x", "y", "material_name"), &CASWorld::set_cell);
	ClassDB::bind_method(D_METHOD("get_cell", "x", "y"), &CASWorld::get_cell);

	ClassDB::bind_method(D_METHOD("get_image"), &CASWorld::get_image);
	ClassDB::bind_method(D_METHOD("get_texture"), &CASWorld::get_texture);
	ClassDB::bind_method(D_METHOD("get_particle_count"), &CASWorld::get_particle_count);
	ClassDB::bind_method(D_METHOD("get_registered_material_names"), &CASWorld::get_registered_material_names);

	ClassDB::bind_method(D_METHOD("save_world", "path"), &CASWorld::save_world);
	ClassDB::bind_method(D_METHOD("load_world", "path"), &CASWorld::load_world);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "backend", PROPERTY_HINT_ENUM, "CPU,GPU"), "set_backend", "get_backend");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "force_gpu"), "set_force_gpu", "get_force_gpu");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gpu_fallback_threshold"), "set_gpu_fallback_threshold", "get_gpu_fallback_threshold");

	BIND_ENUM_CONSTANT(BACKEND_CPU);
	BIND_ENUM_CONSTANT(BACKEND_GPU);
}

void CASWorld::set_backend(int p_backend) {
	if (backend == p_backend) {
		return;
	}
	backend = p_backend;
	if (initialized) {
		UtilityFunctions::print("CASWorld: backend changed to ", get_backend_name(), ". Call init() again to apply.");
	}
}

int CASWorld::get_backend() const {
	return backend;
}

void CASWorld::set_force_gpu(bool p_force) {
	backend_policy.force_gpu = p_force;
}

bool CASWorld::get_force_gpu() const {
	return backend_policy.force_gpu;
}

void CASWorld::set_gpu_fallback_threshold(int p_threshold) {
	backend_policy.gpu_fallback_cell_threshold = std::max(0, p_threshold);
}

int CASWorld::get_gpu_fallback_threshold() const {
	return backend_policy.gpu_fallback_cell_threshold;
}

void CASWorld::set_world_size(int p_width, int p_height) {
	width = p_width;
	height = p_height;
}

Vector2i CASWorld::get_world_size() const {
	return Vector2i(width, height);
}

bool CASWorld::_create_simulator() {
	int effective_backend = ca::BackendFactory::select_backend(backend, width * height, backend_policy);

	simulator = ca::BackendFactory::create(effective_backend);
	if (simulator == nullptr) {
		return false;
	}

	if (!simulator->initialize(width, height, registry)) {
		simulator.reset();
		return false;
	}

	// If we requested GPU but the policy/factory selected CPU, keep the
	// requested enum so the user sees the actual active backend.
	backend = effective_backend;
	return true;
}

void CASWorld::init(int p_width, int p_height) {
	width = p_width;
	height = p_height;

	// unique_ptr reset automatically shuts down and deletes the old simulator.
	simulator.reset();
	renderer.reset();

	if (!_create_simulator()) {
		if (backend == BACKEND_GPU) {
			UtilityFunctions::push_warning("CASWorld: GPU simulator failed to initialize; falling back to CPU.");
			backend = BACKEND_CPU;
			if (!_create_simulator()) {
				UtilityFunctions::push_error("CASWorld: failed to initialize CPU fallback simulator.");
				initialized = false;
				return;
			}
		} else {
			UtilityFunctions::push_error("CASWorld: failed to initialize simulator.");
			initialized = false;
			return;
		}
	}

	renderer.mark_dirty();
	initialized = true;
	UtilityFunctions::print("CASWorld initialized: ", width, "x", height, " backend: ", get_backend_name());
}

void CASWorld::clear() {
	if (simulator == nullptr) {
		return;
	}
	simulator->clear();
	renderer.mark_dirty();
}

void CASWorld::update() {
	if (simulator == nullptr) {
		return;
	}
	simulator->update();
	renderer.mark_dirty();
	// If a texture has already been created (e.g. by get_texture()), keep it
	// in sync so callers that only assign the texture once still see updates.
	if (renderer.has_texture()) {
		renderer.get_texture(*simulator);
	}
}

String CASWorld::get_backend_name() const {
	return backend == BACKEND_CPU ? "CPU" : "GPU";
}

void CASWorld::register_material(const Ref<CASMaterial> &p_material) {
	if (p_material.is_null()) {
		return;
	}
	ca::Material mat;
	mat.name = std::string(p_material->get_material_name().utf8().get_data());
	mat.type = static_cast<ca::MaterialType>(p_material->get_material_type());
	mat.color = p_material->get_material_color();
	mat.density = p_material->get_density();
	mat.lifetime = static_cast<uint16_t>(p_material->get_lifetime());
	mat.decay_to = std::string(p_material->get_decay_to().utf8().get_data());
	mat.flammable = p_material->get_flammable();
	mat.burn_to = std::string(p_material->get_burn_to().utf8().get_data());
	mat.corrosive = p_material->get_corrosive();
	mat.corrosion_residue = std::string(p_material->get_corrosion_residue().utf8().get_data());
	mat.corrosion_chance = p_material->get_corrosion_chance();
	mat.explosive = p_material->get_explosive();
	mat.explode_to = std::string(p_material->get_explode_to().utf8().get_data());
	mat.temperature = p_material->get_temperature();
	mat.emit_light = p_material->get_emit_light();
	mat.glow_color = p_material->get_glow_color();
	mat.velocity_x = p_material->get_velocity_x();
	mat.velocity_y = p_material->get_velocity_y();
	mat.melting_point = p_material->get_melting_point();
	mat.boiling_point = p_material->get_boiling_point();
	mat.freeze_point = p_material->get_freeze_point();
	mat.solid_form = std::string(p_material->get_solid_form().utf8().get_data());
	mat.liquid_form = std::string(p_material->get_liquid_form().utf8().get_data());
	mat.gas_form = std::string(p_material->get_gas_form().utf8().get_data());
	mat.thermal_conductivity = p_material->get_thermal_conductivity();

	uint16_t id = registry.register_material(mat);
	if (simulator != nullptr) {
		simulator->on_registry_changed();
	}

	UtilityFunctions::print("Registered material: ", String(mat.name.c_str()), " id=", id);
}

void CASWorld::set_cell(int p_x, int p_y, const String &p_material_name) {
	if (simulator == nullptr) {
		return;
	}
	uint16_t id = registry.get_material_id(std::string(p_material_name.utf8().get_data()));
	simulator->set_cell(p_x, p_y, id);
	renderer.mark_dirty();
}

String CASWorld::get_cell(int p_x, int p_y) const {
	if (simulator == nullptr) {
		return "";
	}
	uint16_t id = simulator->get_cell(p_x, p_y);
	const ca::Material *mat = registry.get_material(id);
	if (mat == nullptr) {
		return "";
	}
	return String(mat->name.c_str());
}

Ref<Image> CASWorld::get_image() const {
	if (simulator == nullptr) {
		return Ref<Image>();
	}
	// WorldRenderer returns a duplicate so callers can freely resize/modify
	// the image without corrupting the internal cache or texture.
	return const_cast<CASWorld *>(this)->renderer.get_image_snapshot(*simulator);
}

Ref<Texture2D> CASWorld::get_texture() {
	if (simulator == nullptr) {
		return Ref<Texture2D>();
	}
	return renderer.get_texture(*simulator);
}

int CASWorld::get_particle_count() const {
	if (simulator == nullptr) {
		return 0;
	}
	return simulator->get_particle_count();
}

PackedStringArray CASWorld::get_registered_material_names() const {
	PackedStringArray names;
	const std::vector<ca::Material> &materials = registry.get_all();
	for (const ca::Material &m : materials) {
		if (!m.name.empty()) {
			names.append(String(m.name.c_str()));
		}
	}
	return names;
}

Error CASWorld::save_world(const String &p_path) const {
	if (simulator == nullptr) {
		return ERR_UNCONFIGURED;
	}
	PackedByteArray data = simulator->serialize();
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::ModeFlags::WRITE);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	f->store_buffer(data);
	f->close();
	return OK;
}

Error CASWorld::load_world(const String &p_path) {
	if (simulator == nullptr) {
		return ERR_UNCONFIGURED;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::ModeFlags::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	PackedByteArray data = f->get_buffer(f->get_length());
	f->close();
	bool ok = simulator->deserialize(data);
	if (!ok) {
		return ERR_INVALID_DATA;
	}
	renderer.mark_dirty();
	return OK;
}
