#ifndef CELLULAR_AUTOMATA_MATERIAL_REGISTRY_H
#define CELLULAR_AUTOMATA_MATERIAL_REGISTRY_H

#include "material.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ca {

class MaterialRegistry {
public:
    static constexpr uint16_t AIR_ID = 0;

private:
    std::vector<Material> materials;
    std::unordered_map<std::string, uint16_t> name_to_id;

public:
    MaterialRegistry();

    uint16_t register_material(const Material &p_material);
    const Material *get_material(uint16_t p_id) const;
    const Material *get_material(const std::string &p_name) const;
    const Material *get_material(const godot::String &p_name) const;
    uint16_t get_material_id(const std::string &p_name) const;
    uint16_t get_material_id(const godot::String &p_name) const;

    bool has_material(const std::string &p_name) const;
    bool has_material(const godot::String &p_name) const;
    size_t count() const { return materials.size(); }

    const std::vector<Material> &get_all() const { return materials; }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_MATERIAL_REGISTRY_H
