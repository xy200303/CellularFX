#ifndef GPU_SIMULATOR_H
#define GPU_SIMULATOR_H

#include "core/isimulator.h"
#include "core/material_registry.h"
#include "gpu/local_rendering_device.h"

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

    bool initialize(int p_width, int p_height, MaterialRegistry &p_registry) override;
    void update() override;
    void set_cell(int p_x, int p_y, uint16_t p_material_id) override;
    uint16_t get_cell(int p_x, int p_y) const override;
    godot::Ref<godot::Image> render_image() override;
    int get_particle_count() const override;
    void clear() override;

    void on_registry_changed() override;

    godot::PackedByteArray serialize() const override;
    bool deserialize(const godot::PackedByteArray &p_data) override;

    bool is_valid() const { return valid; }

private:
    int width = 0;
    int height = 0;
    int total_cells = 0;

    MaterialRegistry *registry = nullptr;
    LocalRenderingDevice rd;

    godot::RID shader;
    godot::RID pipeline;

    godot::RID buf_a;
    godot::RID buf_b;
    bool current_is_a = true;

    godot::RID materials_buf;
    godot::RID claim_buf;
    godot::RID moved_to_buf;
    godot::RID uniform_set_a_to_b;
    godot::RID uniform_set_b_to_a;

    std::vector<uint32_t> cpu_grid;
    std::vector<uint32_t> material_data;
    bool valid = false;
    bool cpu_grid_dirty = false;
    bool gpu_grid_dirty = false;

    uint32_t frame_seed = 0;

    void create_shader();
    void create_uniform_sets();
    void sync_cpu_from_gpu();
    void sync_gpu_from_cpu();
    void update_materials_buffer();
    void free_rid_safely(godot::RID &p_rid);
    void shutdown();

    int cell_index(int p_x, int p_y) const { return p_y * width + p_x; }

    const MaterialRegistry &registry_ref() const {
        static MaterialRegistry s_dummy;
        return registry ? *registry : s_dummy;
    }
};

} // namespace ca

#endif // GPU_SIMULATOR_H
