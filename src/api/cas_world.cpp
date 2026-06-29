#include "cas_world.h"
#include "cas_material.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

CASWorld::CASWorld() {
}

CASWorld::~CASWorld() {
	if (simulator != nullptr) {
		simulator->shutdown();
		delete simulator;
		simulator = nullptr;
	}
	if (texture.is_valid()) {
		texture.unref();
	}
}

void CASWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_backend", "backend"), &CASWorld::set_backend);
	ClassDB::bind_method(D_METHOD("get_backend"), &CASWorld::get_backend);

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

	ClassDB::bind_method(D_METHOD("save_world", "path"), &CASWorld::save_world);
	ClassDB::bind_method(D_METHOD("load_world", "path"), &CASWorld::load_world);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "backend", PROPERTY_HINT_ENUM, "CPU,GPU"), "set_backend", "get_backend");

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

void CASWorld::set_world_size(int p_width, int p_height) {
	width = p_width;
	height = p_height;
}

Vector2i CASWorld::get_world_size() const {
	return Vector2i(width, height);
}

void CASWorld::init(int p_width, int p_height) {
	width = p_width;
	height = p_height;

	if (simulator != nullptr) {
		simulator->shutdown();
		delete simulator;
		simulator = nullptr;
	}

	if (backend == BACKEND_GPU) {
		simulator = new ca::GPUSimulator();
	} else {
		simulator = new ca::CPUSimulator();
	}

	simulator->init(width, height);

	if (backend == BACKEND_GPU) {
		ca::GPUSimulator *gpu_sim = dynamic_cast<ca::GPUSimulator *>(simulator);
		if (gpu_sim == nullptr || !gpu_sim->is_valid()) {
			UtilityFunctions::push_error("CASWorld: GPU backend initialization failed. Falling back to CPU.");
			simulator->shutdown();
			delete simulator;
			simulator = new ca::CPUSimulator();
			simulator->init(width, height);
			backend = BACKEND_CPU;
		}
	}

	texture = ImageTexture::create_from_image(simulator->get_image());
	initialized = true;
	UtilityFunctions::print("CASWorld initialized: ", width, "x", height, " backend: ", get_backend_name());
}

void CASWorld::clear() {
	if (simulator == nullptr) {
		return;
	}
	simulator->clear();
}

void CASWorld::update() {
	if (simulator == nullptr) {
		return;
	}
	simulator->update();
	Ref<Image> img = simulator->get_image();
	if (texture.is_valid()) {
		texture->update(img);
	}
}

String CASWorld::get_backend_name() const {
	return backend == BACKEND_CPU ? "CPU" : "GPU";
}

static ca::MaterialRegistry *get_registry(ca::ISimulator *p_sim) {
	ca::CPUSimulator *cpu_sim = dynamic_cast<ca::CPUSimulator *>(p_sim);
	if (cpu_sim != nullptr) {
		return &cpu_sim->get_registry();
	}
	ca::GPUSimulator *gpu_sim = dynamic_cast<ca::GPUSimulator *>(p_sim);
	if (gpu_sim != nullptr) {
		return &gpu_sim->get_registry();
	}
	return nullptr;
}

void CASWorld::register_material(const Ref<CASMaterial> &p_material) {
	if (p_material.is_null() || simulator == nullptr) {
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

	ca::MaterialRegistry *reg = get_registry(simulator);
	if (reg == nullptr) {
		return;
	}
	uint16_t id = reg->register_material(mat);

	ca::GPUSimulator *gpu_sim = dynamic_cast<ca::GPUSimulator *>(simulator);
	if (gpu_sim != nullptr) {
		gpu_sim->update_materials_buffer();
	}

	UtilityFunctions::print("Registered material: ", String(mat.name.c_str()), " id=", id);
}

void CASWorld::set_cell(int p_x, int p_y, const String &p_material_name) {
	if (simulator == nullptr) {
		return;
	}
	ca::MaterialRegistry *reg = get_registry(simulator);
	if (reg == nullptr) {
		return;
	}
	uint16_t id = reg->get_material_id(std::string(p_material_name.utf8().get_data()));
	simulator->set_cell(p_x, p_y, id);
}

String CASWorld::get_cell(int p_x, int p_y) const {
	if (simulator == nullptr) {
		return "";
	}
	ca::MaterialRegistry *reg = get_registry(const_cast<CASWorld *>(this)->simulator);
	if (reg == nullptr) {
		return "";
	}
	uint16_t id = simulator->get_cell(p_x, p_y);
	const ca::Material *mat = reg->get_material(id);
	return String(mat->name.c_str());
}

Ref<Image> CASWorld::get_image() const {
	if (simulator == nullptr) {
		return Ref<Image>();
	}
	return simulator->get_image();
}

Ref<Texture2D> CASWorld::get_texture() {
	if (simulator == nullptr) {
		return Ref<Texture2D>();
	}
	godot::Ref<godot::Image> img = simulator->get_image();
	if (img.is_null()) {
		return Ref<Texture2D>();
	}
	if (texture.is_null()) {
		texture = godot::ImageTexture::create_from_image(img);
	} else {
		texture->update(img);
	}
	return texture;
}

int CASWorld::get_particle_count() const {
	if (simulator == nullptr) {
		return 0;
	}
	return simulator->get_particle_count();
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
	// Force texture refresh on next access.
	if (texture.is_valid()) {
		texture->update(simulator->get_image());
	}
	return OK;
}
