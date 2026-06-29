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
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CASMaterial::MaterialType)

#endif // CAS_MATERIAL_H
