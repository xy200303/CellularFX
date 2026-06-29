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

    // Lifetime: 0 = infinite, >0 decrements each frame. At 0 the cell becomes empty.
    uint16_t lifetime = 0;
    std::string decay_to; // name of material to become after lifetime expires

    // Fire / burning.
    bool flammable = false;
    std::string burn_to; // name of material when on fire

    // Corrosion (acid).
    bool corrosive = false;
    std::string corrosion_residue; // name of material the target becomes
    float corrosion_chance = 0.1f;

    // Explosion.
    bool explosive = false;
    std::string explode_to; // name of material after exploding

    // Temperature (currently informational; fire materials should be hot).
    int temperature = 20;

    // Emissive lighting.
    bool emit_light = false;
    godot::Color glow_color;

    // Initial velocity bias (currently informational).
    int velocity_x = 0;
    int velocity_y = 0;

    bool is_empty() const { return id == 0; }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_MATERIAL_H
