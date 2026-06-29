#include "gpu_simulator.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <algorithm>

namespace ca {

GPUSimulator::~GPUSimulator() {
    shutdown();
}

void GPUSimulator::free_rid_safely(godot::RID &p_rid) {
    if (rd != nullptr && p_rid.is_valid()) {
        rd->free_rid(p_rid);
    }
    p_rid = godot::RID();
}

void GPUSimulator::create_shader() {
    if (rd == nullptr) {
        return;
    }

    godot::Ref<godot::RDShaderFile> shader_file = godot::ResourceLoader::get_singleton()->load(
        "res://addons/cellular_automata_engine/shaders/ca_update.glsl");
    if (shader_file.is_null()) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to load compute shader.");
        return;
    }

    godot::Ref<godot::RDShaderSPIRV> spirv = shader_file->get_spirv();
    if (spirv.is_null()) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to get SPIR-V from shader file.");
        return;
    }

    shader = rd->shader_create_from_spirv(spirv, "ca_update");
    if (!shader.is_valid()) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to create shader from SPIR-V.");
        return;
    }

    pipeline = rd->compute_pipeline_create(shader);
    if (!rd->compute_pipeline_is_valid(pipeline)) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to create compute pipeline.");
        free_rid_safely(shader);
    }
}

void GPUSimulator::update_materials_buffer() {
    if (rd == nullptr) {
        return;
    }

    size_t count = registry.count();
    if (count > MAX_MATERIALS) {
        godot::UtilityFunctions::push_warning("GPUSimulator: material count exceeds MAX_MATERIALS (", static_cast<int>(MAX_MATERIALS), "). Extra materials will be ignored by the GPU.");
        count = MAX_MATERIALS;
    }

    // Layout in GPU: uvec4 data[] where data[0].x = count and data[1+i*2..2+i*2] = material i.
    material_data.resize((1 + count * MATERIAL_UVEC4) * MATERIAL_FLOATS);
    memset(material_data.data(), 0, material_data.size() * 4);
    material_data[0] = static_cast<uint32_t>(count);
    for (size_t i = 0; i < count; i++) {
        const Material *m = registry.get_material(static_cast<uint16_t>(i));
        uint16_t burn_to_id = registry.get_material_id(m->burn_to);
        uint16_t explode_to_id = registry.get_material_id(m->explode_to);
        uint16_t decay_to_id = registry.get_material_id(m->decay_to);
        uint16_t residue_id = registry.get_material_id(m->corrosion_residue);
        uint32_t flags = 0u;
        if (m->flammable) flags |= 1u << 0u;
        if (m->corrosive) flags |= 1u << 1u;
        if (m->explosive) flags |= 1u << 2u;
        uint32_t chance_u8 = static_cast<uint32_t>(std::min(m->corrosion_chance, 1.0f) * 255.0f);

        size_t base = (1 + i * MATERIAL_UVEC4) * MATERIAL_FLOATS;
        material_data[base + 0] = (static_cast<uint32_t>(m->type) & 0xFFu)
                                  | ((static_cast<uint32_t>(m->density) & 0xFFu) << 8u)
                                  | ((static_cast<uint32_t>(m->lifetime) & 0xFFFFu) << 16u);
        material_data[base + 1] = (static_cast<uint32_t>(burn_to_id) & 0xFFFFu)
                                  | ((static_cast<uint32_t>(explode_to_id) & 0xFFFFu) << 16u);
        material_data[base + 2] = (static_cast<uint32_t>(decay_to_id) & 0xFFFFu)
                                  | ((static_cast<uint32_t>(residue_id) & 0xFFFFu) << 16u);
        material_data[base + 3] = flags
                                  | ((chance_u8 & 0xFFu) << 8u);

        // Second uvec4: temperature, glow color, emit flag, velocity.
        auto pack_color8 = [](const godot::Color &c) -> uint32_t {
            uint32_t r = static_cast<uint32_t>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
            uint32_t g = static_cast<uint32_t>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
            uint32_t b = static_cast<uint32_t>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
            uint32_t a = static_cast<uint32_t>(std::clamp(c.a, 0.0f, 1.0f) * 255.0f);
            return (r & 0xFFu) | ((g & 0xFFu) << 8u) | ((b & 0xFFu) << 16u) | ((a & 0xFFu) << 24u);
        };
        material_data[base + 4] = static_cast<uint32_t>(m->temperature);
        material_data[base + 5] = pack_color8(m->glow_color);
        material_data[base + 6] = m->emit_light ? 1u : 0u;
        material_data[base + 7] = (static_cast<uint32_t>(m->velocity_x) & 0xFFFFu)
                                  | ((static_cast<uint32_t>(m->velocity_y) & 0xFFFFu) << 16u);
    }

    // Pack data into the pre-allocated buffer region.
    uint32_t buffer_size = (1 + MAX_MATERIALS * MATERIAL_UVEC4) * 4 * MATERIAL_FLOATS;
    godot::PackedByteArray data;
    data.resize(static_cast<int32_t>(buffer_size));
    uint8_t *ptr = data.ptrw();
    memset(ptr, 0, buffer_size);
    if (!material_data.empty()) {
        memcpy(ptr, material_data.data(), material_data.size() * 4);
    }

    if (!materials_buf.is_valid()) {
        materials_buf = rd->storage_buffer_create(buffer_size, data);
    } else {
        rd->buffer_update(materials_buf, 0, buffer_size, data);
    }
}

void GPUSimulator::init(int p_width, int p_height) {
    shutdown();

    width = p_width;
    height = p_height;
    total_cells = width * height;

    rd = godot::RenderingServer::get_singleton()->create_local_rendering_device();
    if (rd == nullptr) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to create local RenderingDevice. GPU backend requires a windowed/GPU-capable Godot process.");
        shutdown();
        return;
    }
    rd_owned = true;

    create_shader();
    if (!pipeline.is_valid()) {
        godot::UtilityFunctions::push_error("GPUSimulator: failed to create compute pipeline.");
        shutdown();
        return;
    }

    uint32_t size_bytes = static_cast<uint32_t>(total_cells * sizeof(uint32_t));
    godot::PackedByteArray empty_data;
    empty_data.resize(static_cast<int32_t>(size_bytes));
    memset(empty_data.ptrw(), 0, size_bytes);

    buf_a = rd->storage_buffer_create(size_bytes, empty_data);
    buf_b = rd->storage_buffer_create(size_bytes, empty_data);

    cpu_grid.resize(total_cells, 0);
    gpu_grid_dirty = false;
    cpu_grid_dirty = false;
    current_is_a = true;
    frame_seed = 0;
    valid = true;

    // Pre-create an empty material buffer so update_materials_buffer can update it in place.
    materials_buf = rd->storage_buffer_create((1 + MAX_MATERIALS * MATERIAL_UVEC4) * 4 * MATERIAL_FLOATS);
    update_materials_buffer();
    create_uniform_sets();
}

void GPUSimulator::create_uniform_sets() {
    if (rd == nullptr || !shader.is_valid() || !buf_a.is_valid() || !buf_b.is_valid()) {
        return;
    }

    free_rid_safely(uniform_set_a_to_b);
    free_rid_safely(uniform_set_b_to_a);

    auto make_uniform = [&](int p_binding, godot::RID p_rid) -> godot::Ref<godot::RDUniform> {
        godot::Ref<godot::RDUniform> u;
        u.instantiate();
        u->set_uniform_type(godot::RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        u->set_binding(p_binding);
        u->add_id(p_rid);
        return u;
    };

    {
        godot::TypedArray<godot::RDUniform> uniforms;
        uniforms.push_back(make_uniform(0, buf_a));
        uniforms.push_back(make_uniform(1, buf_b));
        uniforms.push_back(make_uniform(2, materials_buf));
        uniform_set_a_to_b = rd->uniform_set_create(uniforms, shader, 0);
    }
    {
        godot::TypedArray<godot::RDUniform> uniforms;
        uniforms.push_back(make_uniform(0, buf_b));
        uniforms.push_back(make_uniform(1, buf_a));
        uniforms.push_back(make_uniform(2, materials_buf));
        uniform_set_b_to_a = rd->uniform_set_create(uniforms, shader, 0);
    }
}

void GPUSimulator::sync_gpu_from_cpu() {
    if (rd == nullptr || total_cells == 0) {
        return;
    }
    godot::PackedByteArray data;
    data.resize(total_cells * 4);
    memcpy(data.ptrw(), cpu_grid.data(), total_cells * 4);
    godot::RID current_buf = current_is_a ? buf_a : buf_b;
    rd->buffer_update(current_buf, 0, static_cast<uint32_t>(data.size()), data);
    gpu_grid_dirty = false;
}

void GPUSimulator::sync_cpu_from_gpu() {
    if (rd == nullptr || total_cells == 0) {
        return;
    }
    godot::RID current_buf = current_is_a ? buf_a : buf_b;
    godot::PackedByteArray data = rd->buffer_get_data(current_buf, 0, total_cells * 4);
    if (data.size() >= total_cells * 4) {
        memcpy(cpu_grid.data(), data.ptr(), total_cells * 4);
    }
    cpu_grid_dirty = false;
}

void GPUSimulator::update() {
    if (!valid || rd == nullptr || !pipeline.is_valid()) {
        return;
    }

    if (gpu_grid_dirty) {
        sync_gpu_from_cpu();
    }

    const int PASSES = 3;
    for (int pass = 0; pass < PASSES; pass++) {
        godot::RID set = (current_is_a == (pass % 2 == 0)) ? uniform_set_a_to_b : uniform_set_b_to_a;

        godot::PackedByteArray push;
        push.resize(16);
        int32_t *p = reinterpret_cast<int32_t *>(push.ptrw());
        p[0] = width;
        p[1] = height;
        p[2] = pass;
        p[3] = static_cast<int32_t>(frame_seed++);

        int64_t list = rd->compute_list_begin();
        rd->compute_list_bind_compute_pipeline(list, pipeline);
        rd->compute_list_bind_uniform_set(list, set, 0);
        rd->compute_list_set_push_constant(list, push, static_cast<uint32_t>(push.size()));
        rd->compute_list_dispatch(list, (total_cells + 63) / 64, 1, 1);
        rd->compute_list_end();
        rd->submit();
        rd->sync();
    }

    current_is_a = !current_is_a;
    cpu_grid_dirty = true;
}

void GPUSimulator::set_cell(int p_x, int p_y, uint16_t p_material_id) {
    if (p_x < 0 || p_x >= width || p_y < 0 || p_y >= height) {
        return;
    }
    cpu_grid[cell_index(p_x, p_y)] = p_material_id;
    gpu_grid_dirty = true;
}

uint16_t GPUSimulator::get_cell(int p_x, int p_y) const {
    if (p_x < 0 || p_x >= width || p_y < 0 || p_y >= height) {
        return 0;
    }
    if (cpu_grid_dirty) {
        const_cast<GPUSimulator *>(this)->sync_cpu_from_gpu();
    }
    return static_cast<uint16_t>(cpu_grid[cell_index(p_x, p_y)]);
}

godot::Ref<godot::Image> GPUSimulator::get_image() {
    if (cpu_grid_dirty) {
        sync_cpu_from_gpu();
    }

    godot::Ref<godot::Image> img = godot::Image::create(width, height, false, godot::Image::FORMAT_RGBA8);
    img->fill(godot::Color(0, 0, 0, 0));

    const std::vector<Material> &materials = registry.get_all();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t mat_id = static_cast<uint16_t>(cpu_grid[cell_index(x, y)]);
            if (mat_id != 0 && mat_id < materials.size()) {
                const Material &mat = materials[mat_id];
                if (mat.emit_light) {
                    img->set_pixel(x, y, mat.glow_color);
                } else {
                    img->set_pixel(x, y, mat.color);
                }
            }
        }
    }
    return img;
}

int GPUSimulator::get_particle_count() const {
    if (cpu_grid_dirty) {
        const_cast<GPUSimulator *>(this)->sync_cpu_from_gpu();
    }
    int count = 0;
    for (uint32_t id : cpu_grid) {
        if (id != 0) {
            count++;
        }
    }
    return count;
}

void GPUSimulator::clear() {
    std::fill(cpu_grid.begin(), cpu_grid.end(), 0u);
    gpu_grid_dirty = true;
}

void GPUSimulator::shutdown() {
    valid = false;
    free_rid_safely(uniform_set_a_to_b);
    free_rid_safely(uniform_set_b_to_a);
    free_rid_safely(buf_a);
    free_rid_safely(buf_b);
    free_rid_safely(materials_buf);
    free_rid_safely(pipeline);
    free_rid_safely(shader);

    if (rd != nullptr && rd_owned) {
        // RenderingDevice local device does not need explicit free in this API.
        rd = nullptr;
    }
    rd_owned = false;

    width = 0;
    height = 0;
    total_cells = 0;
    cpu_grid.clear();
    material_data.clear();
    cpu_grid_dirty = false;
    gpu_grid_dirty = false;
    current_is_a = true;
    frame_seed = 0;
}

} // namespace ca
