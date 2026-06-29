#ifndef GPU_SIMULATOR_H
#define GPU_SIMULATOR_H

#include "core/isimulator.h"
#include "core/material_registry.h"

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <cstdint>
#include <vector>

namespace ca {

class GPUSimulator : public ISimulator {
public:
    static constexpr uint32_t MAX_MATERIALS = 256;
    static constexpr uint32_t MATERIAL_FLOATS = 4;
    static constexpr uint32_t MATERIAL_UVEC4 = 3;

    GPUSimulator() = default;
    ~GPUSimulator() override;

    void init(int p_width, int p_height) override;
    void update() override;
    void set_cell(int p_x, int p_y, uint16_t p_material_id) override;
    uint16_t get_cell(int p_x, int p_y) const override;
    godot::Ref<godot::Image> get_image() override;
    int get_particle_count() const override;
    bool is_valid() const { return valid; }
    void clear() override;
    void shutdown() override;

    godot::PackedByteArray serialize() const override;
    bool deserialize(const godot::PackedByteArray &p_data) override;

    MaterialRegistry &get_registry() { return registry; }
    const MaterialRegistry &get_registry() const { return registry; }
    void update_materials_buffer();

private:
    int width = 0;
    int height = 0;
    int total_cells = 0;

    godot::RenderingDevice *rd = nullptr;
    bool rd_owned = false;

    godot::RID shader;
    godot::RID pipeline;

    godot::RID buf_a;
    godot::RID buf_b;
    bool current_is_a = true;

    godot::RID materials_buf;
    godot::RID uniform_set_a_to_b;
    godot::RID uniform_set_b_to_a;

    std::vector<uint32_t> cpu_grid;
    std::vector<uint32_t> material_data;
    bool valid = false;
    bool cpu_grid_dirty = false;
    bool gpu_grid_dirty = false;

    MaterialRegistry registry;
    uint32_t frame_seed = 0;

    void create_shader();
    void create_uniform_sets();
    void sync_cpu_from_gpu();
    void sync_gpu_from_cpu();
    int cell_index(int p_x, int p_y) const { return p_y * width + p_x; }
    void free_rid_safely(godot::RID &p_rid);
};

} // namespace ca

#endif // GPU_SIMULATOR_H
