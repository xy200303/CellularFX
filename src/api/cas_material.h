#ifndef CAS_MATERIAL_H
#define CAS_MATERIAL_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class CASMaterial : public Resource {
    GDCLASS(CASMaterial, Resource)

public:
    enum MaterialType {
        TYPE_SOLID = 0,
        TYPE_POWDER = 1,
        TYPE_LIQUID = 2,
        TYPE_GAS = 3
    };

private:
    String material_name = "material";
    int material_type = TYPE_POWDER;
    Color material_color = Color(1, 1, 1, 1);
    int density = 1;

    // Lifetime and decay.
    int lifetime = 0;
    String decay_to = "";

    // Burning.
    bool flammable = false;
    String burn_to = "";

    // Corrosion (acid).
    bool corrosive = false;
    String corrosion_residue = "";
    float corrosion_chance = 0.1f;

    // Explosion.
    bool explosive = false;
    String explode_to = "";

    // Temperature and visual effects.
    int temperature = 20;
    bool emit_light = false;
    Color glow_color = Color(1, 1, 1, 1);
    int velocity_x = 0;
    int velocity_y = 0;

protected:
    static void _bind_methods();

public:
    void set_material_name(const String &p_name);
    String get_material_name() const;

    void set_material_type(int p_type);
    int get_material_type() const;

    void set_material_color(const Color &p_color);
    Color get_material_color() const;

    void set_density(int p_density);
    int get_density() const;

    void set_lifetime(int p_lifetime);
    int get_lifetime() const;

    void set_decay_to(const String &p_name);
    String get_decay_to() const;

    void set_flammable(bool p_value);
    bool get_flammable() const;

    void set_burn_to(const String &p_name);
    String get_burn_to() const;

    void set_corrosive(bool p_value);
    bool get_corrosive() const;

    void set_corrosion_residue(const String &p_name);
    String get_corrosion_residue() const;

    void set_corrosion_chance(float p_value);
    float get_corrosion_chance() const;

    void set_explosive(bool p_value);
    bool get_explosive() const;

    void set_explode_to(const String &p_name);
    String get_explode_to() const;

    void set_temperature(int p_value);
    int get_temperature() const;

    void set_emit_light(bool p_value);
    bool get_emit_light() const;

    void set_glow_color(const Color &p_color);
    Color get_glow_color() const;

    void set_velocity_x(int p_value);
    int get_velocity_x() const;

    void set_velocity_y(int p_value);
    int get_velocity_y() const;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CASMaterial::MaterialType)

#endif // CAS_MATERIAL_H
