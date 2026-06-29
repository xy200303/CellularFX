#ifndef CELLULAR_AUTOMATA_ISIMULATOR_H
#define CELLULAR_AUTOMATA_ISIMULATOR_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace ca {

class ISimulator {
public:
    virtual ~ISimulator() = default;

    virtual void init(int p_width, int p_height) = 0;
    virtual void update() = 0;
    virtual void set_cell(int p_x, int p_y, uint16_t p_material_id) = 0;
    virtual uint16_t get_cell(int p_x, int p_y) const = 0;
    virtual godot::Ref<godot::Image> get_image() = 0;
    virtual int get_particle_count() const = 0;
    virtual void clear() = 0;
    virtual void shutdown() = 0;
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_ISIMULATOR_H
