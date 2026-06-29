#include "cas_material.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void CASMaterial::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_material_name", "name"), &CASMaterial::set_material_name);
    ClassDB::bind_method(D_METHOD("get_material_name"), &CASMaterial::get_material_name);

    ClassDB::bind_method(D_METHOD("set_material_type", "type"), &CASMaterial::set_material_type);
    ClassDB::bind_method(D_METHOD("get_material_type"), &CASMaterial::get_material_type);

    ClassDB::bind_method(D_METHOD("set_material_color", "color"), &CASMaterial::set_material_color);
    ClassDB::bind_method(D_METHOD("get_material_color"), &CASMaterial::get_material_color);

    ClassDB::bind_method(D_METHOD("set_density", "density"), &CASMaterial::set_density);
    ClassDB::bind_method(D_METHOD("get_density"), &CASMaterial::get_density);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "material_name"), "set_material_name", "get_material_name");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "material_type", PROPERTY_HINT_ENUM, "Solid,Powder,Liquid,Gas"), "set_material_type", "get_material_type");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "material_color"), "set_material_color", "get_material_color");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "density"), "set_density", "get_density");

    BIND_ENUM_CONSTANT(TYPE_SOLID);
    BIND_ENUM_CONSTANT(TYPE_POWDER);
    BIND_ENUM_CONSTANT(TYPE_LIQUID);
    BIND_ENUM_CONSTANT(TYPE_GAS);
}

void CASMaterial::set_material_name(const String &p_name) {
    material_name = p_name;
}

String CASMaterial::get_material_name() const {
    return material_name;
}

void CASMaterial::set_material_type(int p_type) {
    material_type = p_type;
}

int CASMaterial::get_material_type() const {
    return material_type;
}

void CASMaterial::set_material_color(const Color &p_color) {
    material_color = p_color;
}

Color CASMaterial::get_material_color() const {
    return material_color;
}

void CASMaterial::set_density(int p_density) {
    density = p_density;
}

int CASMaterial::get_density() const {
    return density;
}
