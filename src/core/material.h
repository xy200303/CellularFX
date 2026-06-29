#ifndef CELLULAR_AUTOMATA_MATERIAL_H
#define CELLULAR_AUTOMATA_MATERIAL_H

#include <cstdint>
#include <string>

#include <godot_cpp/variant/color.hpp>

namespace ca {

enum class MaterialType : uint8_t {
    SOLID = 0,
    POWDER = 1,
    LIQUID = 2,
    GAS = 3
};

struct Material {
    uint16_t id = 0;
    std::string name;
    MaterialType type = MaterialType::SOLID;
    godot::Color color;
    int density = 0;

    bool is_empty() const { return id == 0; }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_MATERIAL_H
