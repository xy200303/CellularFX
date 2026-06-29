#include "material_registry.h"

#include <godot_cpp/variant/string.hpp>

namespace ca {

MaterialRegistry::MaterialRegistry() {
    // Register air as material 0.
    Material air;
    air.id = AIR_ID;
    air.name = "air";
    air.type = MaterialType::SOLID;
    air.color = godot::Color(0, 0, 0, 0);
    air.density = 0;
    materials.push_back(air);
    name_to_id[air.name] = AIR_ID;
}

uint16_t MaterialRegistry::register_material(const Material &p_material) {
    uint16_t id = static_cast<uint16_t>(materials.size());
    if (id > 65534) {
        return AIR_ID; // Too many materials.
    }
    Material mat = p_material;
    mat.id = id;
    materials.push_back(mat);
    name_to_id[mat.name] = id;
    return id;
}

const Material *MaterialRegistry::get_material(uint16_t p_id) const {
    if (p_id >= materials.size()) {
        return &materials[AIR_ID];
    }
    return &materials[p_id];
}

const Material *MaterialRegistry::get_material(const std::string &p_name) const {
    auto it = name_to_id.find(p_name);
    if (it == name_to_id.end()) {
        return &materials[AIR_ID];
    }
    return &materials[it->second];
}

const Material *MaterialRegistry::get_material(const godot::String &p_name) const {
    return get_material(std::string(p_name.utf8().get_data()));
}

uint16_t MaterialRegistry::get_material_id(const std::string &p_name) const {
    auto it = name_to_id.find(p_name);
    if (it == name_to_id.end()) {
        return AIR_ID;
    }
    return it->second;
}

uint16_t MaterialRegistry::get_material_id(const godot::String &p_name) const {
    return get_material_id(std::string(p_name.utf8().get_data()));
}

bool MaterialRegistry::has_material(const std::string &p_name) const {
    return name_to_id.find(p_name) != name_to_id.end();
}

bool MaterialRegistry::has_material(const godot::String &p_name) const {
    return has_material(std::string(p_name.utf8().get_data()));
}

} // namespace ca
