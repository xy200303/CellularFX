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

    ClassDB::bind_method(D_METHOD("set_lifetime", "lifetime"), &CASMaterial::set_lifetime);
    ClassDB::bind_method(D_METHOD("get_lifetime"), &CASMaterial::get_lifetime);

    ClassDB::bind_method(D_METHOD("set_decay_to", "name"), &CASMaterial::set_decay_to);
    ClassDB::bind_method(D_METHOD("get_decay_to"), &CASMaterial::get_decay_to);

    ClassDB::bind_method(D_METHOD("set_flammable", "value"), &CASMaterial::set_flammable);
    ClassDB::bind_method(D_METHOD("get_flammable"), &CASMaterial::get_flammable);

    ClassDB::bind_method(D_METHOD("set_burn_to", "name"), &CASMaterial::set_burn_to);
    ClassDB::bind_method(D_METHOD("get_burn_to"), &CASMaterial::get_burn_to);

    ClassDB::bind_method(D_METHOD("set_corrosive", "value"), &CASMaterial::set_corrosive);
    ClassDB::bind_method(D_METHOD("get_corrosive"), &CASMaterial::get_corrosive);

    ClassDB::bind_method(D_METHOD("set_corrosion_residue", "name"), &CASMaterial::set_corrosion_residue);
    ClassDB::bind_method(D_METHOD("get_corrosion_residue"), &CASMaterial::get_corrosion_residue);

    ClassDB::bind_method(D_METHOD("set_corrosion_chance", "value"), &CASMaterial::set_corrosion_chance);
    ClassDB::bind_method(D_METHOD("get_corrosion_chance"), &CASMaterial::get_corrosion_chance);

    ClassDB::bind_method(D_METHOD("set_explosive", "value"), &CASMaterial::set_explosive);
    ClassDB::bind_method(D_METHOD("get_explosive"), &CASMaterial::get_explosive);

    ClassDB::bind_method(D_METHOD("set_explode_to", "name"), &CASMaterial::set_explode_to);
    ClassDB::bind_method(D_METHOD("get_explode_to"), &CASMaterial::get_explode_to);

    ClassDB::bind_method(D_METHOD("set_temperature", "value"), &CASMaterial::set_temperature);
    ClassDB::bind_method(D_METHOD("get_temperature"), &CASMaterial::get_temperature);

    ClassDB::bind_method(D_METHOD("set_emit_light", "value"), &CASMaterial::set_emit_light);
    ClassDB::bind_method(D_METHOD("get_emit_light"), &CASMaterial::get_emit_light);

    ClassDB::bind_method(D_METHOD("set_glow_color", "color"), &CASMaterial::set_glow_color);
    ClassDB::bind_method(D_METHOD("get_glow_color"), &CASMaterial::get_glow_color);

    ClassDB::bind_method(D_METHOD("set_velocity_x", "value"), &CASMaterial::set_velocity_x);
    ClassDB::bind_method(D_METHOD("get_velocity_x"), &CASMaterial::get_velocity_x);

    ClassDB::bind_method(D_METHOD("set_velocity_y", "value"), &CASMaterial::set_velocity_y);
    ClassDB::bind_method(D_METHOD("get_velocity_y"), &CASMaterial::get_velocity_y);

    ClassDB::bind_method(D_METHOD("set_melting_point", "value"), &CASMaterial::set_melting_point);
    ClassDB::bind_method(D_METHOD("get_melting_point"), &CASMaterial::get_melting_point);

    ClassDB::bind_method(D_METHOD("set_boiling_point", "value"), &CASMaterial::set_boiling_point);
    ClassDB::bind_method(D_METHOD("get_boiling_point"), &CASMaterial::get_boiling_point);

    ClassDB::bind_method(D_METHOD("set_freeze_point", "value"), &CASMaterial::set_freeze_point);
    ClassDB::bind_method(D_METHOD("get_freeze_point"), &CASMaterial::get_freeze_point);

    ClassDB::bind_method(D_METHOD("set_solid_form", "name"), &CASMaterial::set_solid_form);
    ClassDB::bind_method(D_METHOD("get_solid_form"), &CASMaterial::get_solid_form);

    ClassDB::bind_method(D_METHOD("set_liquid_form", "name"), &CASMaterial::set_liquid_form);
    ClassDB::bind_method(D_METHOD("get_liquid_form"), &CASMaterial::get_liquid_form);

    ClassDB::bind_method(D_METHOD("set_gas_form", "name"), &CASMaterial::set_gas_form);
    ClassDB::bind_method(D_METHOD("get_gas_form"), &CASMaterial::get_gas_form);

    ClassDB::bind_method(D_METHOD("set_thermal_conductivity", "value"), &CASMaterial::set_thermal_conductivity);
    ClassDB::bind_method(D_METHOD("get_thermal_conductivity"), &CASMaterial::get_thermal_conductivity);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "material_name"), "set_material_name", "get_material_name");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "material_type", PROPERTY_HINT_ENUM, "Solid,Powder,Liquid,Gas"), "set_material_type", "get_material_type");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "material_color"), "set_material_color", "get_material_color");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "density"), "set_density", "get_density");

    ADD_GROUP("Lifetime", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "lifetime"), "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "decay_to"), "set_decay_to", "get_decay_to");

    ADD_GROUP("Burning", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flammable"), "set_flammable", "get_flammable");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "burn_to"), "set_burn_to", "get_burn_to");

    ADD_GROUP("Corrosion", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "corrosive"), "set_corrosive", "get_corrosive");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "corrosion_residue"), "set_corrosion_residue", "get_corrosion_residue");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "corrosion_chance"), "set_corrosion_chance", "get_corrosion_chance");

    ADD_GROUP("Explosion", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "explosive"), "set_explosive", "get_explosive");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "explode_to"), "set_explode_to", "get_explode_to");

    ADD_GROUP("Temperature & Effects", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "temperature"), "set_temperature", "get_temperature");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emit_light"), "set_emit_light", "get_emit_light");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "glow_color"), "set_glow_color", "get_glow_color");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "velocity_x"), "set_velocity_x", "get_velocity_x");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "velocity_y"), "set_velocity_y", "get_velocity_y");

    ADD_GROUP("Phase Change", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "melting_point"), "set_melting_point", "get_melting_point");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "boiling_point"), "set_boiling_point", "get_boiling_point");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "freeze_point"), "set_freeze_point", "get_freeze_point");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "solid_form"), "set_solid_form", "get_solid_form");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "liquid_form"), "set_liquid_form", "get_liquid_form");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "gas_form"), "set_gas_form", "get_gas_form");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "thermal_conductivity"), "set_thermal_conductivity", "get_thermal_conductivity");

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

void CASMaterial::set_lifetime(int p_lifetime) {
    lifetime = p_lifetime;
}

int CASMaterial::get_lifetime() const {
    return lifetime;
}

void CASMaterial::set_decay_to(const String &p_name) {
    decay_to = p_name;
}

String CASMaterial::get_decay_to() const {
    return decay_to;
}

void CASMaterial::set_flammable(bool p_value) {
    flammable = p_value;
}

bool CASMaterial::get_flammable() const {
    return flammable;
}

void CASMaterial::set_burn_to(const String &p_name) {
    burn_to = p_name;
}

String CASMaterial::get_burn_to() const {
    return burn_to;
}

void CASMaterial::set_corrosive(bool p_value) {
    corrosive = p_value;
}

bool CASMaterial::get_corrosive() const {
    return corrosive;
}

void CASMaterial::set_corrosion_residue(const String &p_name) {
    corrosion_residue = p_name;
}

String CASMaterial::get_corrosion_residue() const {
    return corrosion_residue;
}

void CASMaterial::set_corrosion_chance(float p_value) {
    corrosion_chance = p_value;
}

float CASMaterial::get_corrosion_chance() const {
    return corrosion_chance;
}

void CASMaterial::set_explosive(bool p_value) {
    explosive = p_value;
}

bool CASMaterial::get_explosive() const {
    return explosive;
}

void CASMaterial::set_explode_to(const String &p_name) {
    explode_to = p_name;
}

String CASMaterial::get_explode_to() const {
    return explode_to;
}

void CASMaterial::set_temperature(int p_value) {
    temperature = p_value;
}

int CASMaterial::get_temperature() const {
    return temperature;
}

void CASMaterial::set_emit_light(bool p_value) {
    emit_light = p_value;
}

bool CASMaterial::get_emit_light() const {
    return emit_light;
}

void CASMaterial::set_glow_color(const Color &p_color) {
    glow_color = p_color;
}

Color CASMaterial::get_glow_color() const {
    return glow_color;
}

void CASMaterial::set_velocity_x(int p_value) {
    velocity_x = p_value;
}

int CASMaterial::get_velocity_x() const {
    return velocity_x;
}

void CASMaterial::set_velocity_y(int p_value) {
    velocity_y = p_value;
}

int CASMaterial::get_velocity_y() const {
    return velocity_y;
}

void CASMaterial::set_melting_point(int p_value) {
    melting_point = p_value;
}

int CASMaterial::get_melting_point() const {
    return melting_point;
}

void CASMaterial::set_boiling_point(int p_value) {
    boiling_point = p_value;
}

int CASMaterial::get_boiling_point() const {
    return boiling_point;
}

void CASMaterial::set_freeze_point(int p_value) {
    freeze_point = p_value;
}

int CASMaterial::get_freeze_point() const {
    return freeze_point;
}

void CASMaterial::set_solid_form(const String &p_name) {
    solid_form = p_name;
}

String CASMaterial::get_solid_form() const {
    return solid_form;
}

void CASMaterial::set_liquid_form(const String &p_name) {
    liquid_form = p_name;
}

String CASMaterial::get_liquid_form() const {
    return liquid_form;
}

void CASMaterial::set_gas_form(const String &p_name) {
    gas_form = p_name;
}

String CASMaterial::get_gas_form() const {
    return gas_form;
}

void CASMaterial::set_thermal_conductivity(int p_value) {
    thermal_conductivity = p_value;
}

int CASMaterial::get_thermal_conductivity() const {
    return thermal_conductivity;
}
