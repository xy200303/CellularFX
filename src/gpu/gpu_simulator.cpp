#include "gpu_simulator.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <algorithm>
#include <cstring>

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

        // Third uvec4: phase-change parameters.
        uint16_t solid_id = registry.get_material_id(m->solid_form);
        uint16_t liquid_id = registry.get_material_id(m->liquid_form);
        uint16_t gas_id = registry.get_material_id(m->gas_form);
        material_data[base + 8] = (static_cast<uint32_t>(static_cast<int16_t>(m->melting_point)) & 0xFFFFu)
                                  | ((static_cast<uint32_t>(static_cast<int16_t>(m->boiling_point)) & 0xFFFFu) << 16u);
        material_data[base + 9] = (static_cast<uint32_t>(static_cast<int16_t>(m->freeze_point)) & 0xFFFFu)
                                  | ((static_cast<uint32_t>(std::clamp(m->thermal_conductivity, 0, 65535)) & 0xFFFFu) << 16u);
        material_data[base + 10] = (static_cast<uint32_t>(solid_id) & 0xFFFFu)
                                   | ((static_cast<uint32_t>(liquid_id) & 0xFFFFu) << 16u);
        material_data[base + 11] = (static_cast<uint32_t>(gas_id) & 0xFFFFu);
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

    uint32_t size_bytes = static_cast<uint32_t>(total_cells * sizeof(uint32_t) * 2);
    godot::PackedByteArray empty_data;
    empty_data.resize(static_cast<int32_t>(size_bytes));
    memset(empty_data.ptrw(), 0, size_bytes);

    buf_a = rd->storage_buffer_create(size_bytes, empty_data);
    buf_b = rd->storage_buffer_create(size_bytes, empty_data);

    uint32_t claim_size = static_cast<uint32_t>(total_cells * sizeof(uint32_t));
    godot::PackedByteArray claim_empty;
    claim_empty.resize(static_cast<int32_t>(claim_size));
    memset(claim_empty.ptrw(), 0, claim_size);
    claim_buf = rd->storage_buffer_create(claim_size, claim_empty);
    moved_to_buf = rd->storage_buffer_create(claim_size, claim_empty);

    cpu_grid.resize(static_cast<size_t>(total_cells) * 2, 0);
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
    if (rd == nullptr || !shader.is_valid() || !buf_a.is_valid() || !buf_b.is_valid() || !claim_buf.is_valid() || !moved_to_buf.is_valid()) {
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
        uniforms.push_back(make_uniform(3, claim_buf));
        uniforms.push_back(make_uniform(4, moved_to_buf));
        uniform_set_a_to_b = rd->uniform_set_create(uniforms, shader, 0);
    }
    {
        godot::TypedArray<godot::RDUniform> uniforms;
        uniforms.push_back(make_uniform(0, buf_b));
        uniforms.push_back(make_uniform(1, buf_a));
        uniforms.push_back(make_uniform(2, materials_buf));
        uniforms.push_back(make_uniform(3, claim_buf));
        uniforms.push_back(make_uniform(4, moved_to_buf));
        uniform_set_b_to_a = rd->uniform_set_create(uniforms, shader, 0);
    }
}

void GPUSimulator::sync_gpu_from_cpu() {
    if (rd == nullptr || total_cells == 0) {
        return;
    }
    godot::PackedByteArray data;
    data.resize(total_cells * 8);
    memcpy(data.ptrw(), cpu_grid.data(), total_cells * 8);
    godot::RID current_buf = current_is_a ? buf_a : buf_b;
    rd->buffer_update(current_buf, 0, static_cast<uint32_t>(data.size()), data);
    gpu_grid_dirty = false;
}

void GPUSimulator::sync_cpu_from_gpu() {
    if (rd == nullptr || total_cells == 0) {
        return;
    }
    godot::RID current_buf = current_is_a ? buf_a : buf_b;
    godot::PackedByteArray data = rd->buffer_get_data(current_buf, 0, total_cells * 8);
    if (data.size() >= total_cells * 8) {
        memcpy(cpu_grid.data(), data.ptr(), total_cells * 8);
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

    // Clear per-frame auxiliary buffers on the GPU.
    uint32_t claim_size = static_cast<uint32_t>(total_cells * sizeof(uint32_t));
    rd->buffer_clear(claim_buf, 0, claim_size);
    rd->buffer_clear(moved_to_buf, 0, claim_size);

    const int PASSES = 4;
    godot::RID set = current_is_a ? uniform_set_a_to_b : uniform_set_b_to_a;

    int64_t list = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline(list, pipeline);
    rd->compute_list_bind_uniform_set(list, set, 0);
    for (int pass = 0; pass < PASSES; pass++) {
        godot::PackedByteArray push;
        push.resize(16);
        int32_t *p = reinterpret_cast<int32_t *>(push.ptrw());
        p[0] = width;
        p[1] = height;
        p[2] = pass;
        p[3] = static_cast<int32_t>(frame_seed++);

        rd->compute_list_set_push_constant(list, push, static_cast<uint32_t>(push.size()));
        rd->compute_list_dispatch(list, (total_cells + 63) / 64, 1, 1);
        if (pass != PASSES - 1) {
            rd->compute_list_add_barrier(list);
        }
    }
    rd->compute_list_end();
    rd->submit();
    rd->sync();

    current_is_a = !current_is_a;
    cpu_grid_dirty = true;
}

void GPUSimulator::set_cell(int p_x, int p_y, uint16_t p_material_id) {
    if (p_x < 0 || p_x >= width || p_y < 0 || p_y >= height) {
        return;
    }
    size_t base = static_cast<size_t>(cell_index(p_x, p_y)) * 2;
    const Material *mat = registry.get_material(p_material_id);
    int16_t temp = mat ? static_cast<int16_t>(mat->temperature) : static_cast<int16_t>(0);
    uint16_t temp_u = static_cast<uint16_t>(temp);
    cpu_grid[base + 0] = static_cast<uint32_t>(p_material_id) | (static_cast<uint32_t>(temp_u) << 16u);
    cpu_grid[base + 1] = 0u;
    gpu_grid_dirty = true;
}

uint16_t GPUSimulator::get_cell(int p_x, int p_y) const {
    if (p_x < 0 || p_x >= width || p_y < 0 || p_y >= height) {
        return 0;
    }
    if (cpu_grid_dirty) {
        const_cast<GPUSimulator *>(this)->sync_cpu_from_gpu();
    }
    return static_cast<uint16_t>(cpu_grid[static_cast<size_t>(cell_index(p_x, p_y)) * 2]);
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
            size_t base = static_cast<size_t>(cell_index(x, y)) * 2;
            uint16_t mat_id = static_cast<uint16_t>(cpu_grid[base] & 0xFFFFu);
            if (mat_id != 0 && mat_id < materials.size()) {
                const Material &mat = materials[mat_id];
                uint16_t temp_u = static_cast<uint16_t>(cpu_grid[base] >> 16);
                int16_t temp = static_cast<int16_t>(temp_u);
                godot::Color c = mat.color;
                int temp_diff = static_cast<int>(temp) - mat.temperature;
                if (temp_diff > 10) {
                    float t = static_cast<float>(temp_diff) / 300.0f;
                    t = std::min(t, 1.0f);
                    c = c.lerp(mat.glow_color.a > 0.0f ? mat.glow_color : godot::Color(1.0f, 0.3f, 0.1f), t);
                } else if (temp_diff < -10) {
                    float t = static_cast<float>(-temp_diff) / 100.0f;
                    t = std::min(t, 0.6f);
                    c = c.lerp(godot::Color(0.7f, 0.9f, 1.0f), t);
                }
                if (mat.emit_light) {
                    img->set_pixel(x, y, mat.glow_color.lerp(c, 0.3f));
                } else {
                    img->set_pixel(x, y, c);
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
    for (size_t i = 0; i < cpu_grid.size(); i += 2) {
        if ((cpu_grid[i] & 0xFFFFu) != 0) {
            count++;
        }
    }
    return count;
}

void GPUSimulator::clear() {
    std::fill(cpu_grid.begin(), cpu_grid.end(), 0u);
    if (rd != nullptr) {
        uint32_t size_bytes = static_cast<uint32_t>(total_cells * sizeof(uint32_t) * 2);
        godot::PackedByteArray empty_data;
        empty_data.resize(static_cast<int32_t>(size_bytes));
        memset(empty_data.ptrw(), 0, size_bytes);
        rd->buffer_update(buf_a, 0, size_bytes, empty_data);
        rd->buffer_update(buf_b, 0, size_bytes, empty_data);
        uint32_t claim_size = static_cast<uint32_t>(total_cells * sizeof(uint32_t));
        godot::PackedByteArray claim_empty;
        claim_empty.resize(static_cast<int32_t>(claim_size));
        memset(claim_empty.ptrw(), 0, claim_size);
        rd->buffer_update(claim_buf, 0, claim_size, claim_empty);
        rd->buffer_update(moved_to_buf, 0, claim_size, claim_empty);
    }
    gpu_grid_dirty = false;
    cpu_grid_dirty = false;
}

void GPUSimulator::shutdown() {
    valid = false;
    free_rid_safely(uniform_set_a_to_b);
    free_rid_safely(uniform_set_b_to_a);
    free_rid_safely(buf_a);
    free_rid_safely(buf_b);
    free_rid_safely(claim_buf);
    free_rid_safely(moved_to_buf);
    free_rid_safely(materials_buf);
    free_rid_safely(pipeline);
    free_rid_safely(shader);

    if (rd != nullptr && rd_owned) {
        // The local RenderingDevice is an Object we created; release it to
        // avoid ObjectDB leaks at shutdown.
        godot::memdelete(rd);
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

godot::PackedByteArray GPUSimulator::serialize() const {
    if (cpu_grid_dirty) {
        const_cast<GPUSimulator *>(this)->sync_cpu_from_gpu();
    }

    const std::vector<Material> &materials = registry.get_all();
    godot::PackedByteArray data;

    size_t header_size = 4 + 4 + 4 + 4 + 4;
    size_t material_table_size = 0;
    for (const Material &m : materials) {
        material_table_size += 2 + m.name.size();
    }
    size_t cell_size = 2 + 2 + 1;
    size_t total_size = header_size + material_table_size + static_cast<size_t>(width * height) * cell_size;

    data.resize(static_cast<int32_t>(total_size));
    uint8_t *ptr = data.ptrw();
    size_t offset = 0;

    auto write_u32 = [&](uint32_t v) {
        *reinterpret_cast<uint32_t *>(ptr + offset) = v;
        offset += 4;
    };
    auto write_i32 = [&](int32_t v) {
        *reinterpret_cast<int32_t *>(ptr + offset) = v;
        offset += 4;
    };
    auto write_u16 = [&](uint16_t v) {
        *reinterpret_cast<uint16_t *>(ptr + offset) = v;
        offset += 2;
    };
    auto write_i16 = [&](int16_t v) {
        *reinterpret_cast<int16_t *>(ptr + offset) = v;
        offset += 2;
    };
    auto write_u8 = [&](uint8_t v) {
        ptr[offset] = v;
        offset += 1;
    };

    memcpy(ptr + offset, "CAFX", 4);
    offset += 4;
    write_u32(1);
    write_i32(width);
    write_i32(height);
    write_u32(static_cast<uint32_t>(materials.size()));

    for (const Material &m : materials) {
        write_u16(static_cast<uint16_t>(m.name.size()));
        memcpy(ptr + offset, m.name.data(), m.name.size());
        offset += m.name.size();
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t base = static_cast<size_t>(cell_index(x, y)) * 2;
            uint16_t mat_id = static_cast<uint16_t>(cpu_grid[base] & 0xFFFFu);
            uint16_t temp_u = static_cast<uint16_t>(cpu_grid[base] >> 16);
            int16_t temp = static_cast<int16_t>(temp_u);
            uint8_t flags = static_cast<uint8_t>(cpu_grid[base + 1] & 0xFFu);
            write_u16(mat_id);
            write_i16(temp);
            write_u8(flags);
        }
    }

    return data;
}

bool GPUSimulator::deserialize(const godot::PackedByteArray &p_data) {
    if (p_data.size() < 20) {
        return false;
    }
    const uint8_t *ptr = p_data.ptr();
    size_t offset = 0;

    auto read_u32 = [&]() -> uint32_t {
        uint32_t v = *reinterpret_cast<const uint32_t *>(ptr + offset);
        offset += 4;
        return v;
    };
    auto read_i32 = [&]() -> int32_t {
        int32_t v = *reinterpret_cast<const int32_t *>(ptr + offset);
        offset += 4;
        return v;
    };
    auto read_u16 = [&]() -> uint16_t {
        uint16_t v = *reinterpret_cast<const uint16_t *>(ptr + offset);
        offset += 2;
        return v;
    };
    auto read_i16 = [&]() -> int16_t {
        int16_t v = *reinterpret_cast<const int16_t *>(ptr + offset);
        offset += 2;
        return v;
    };
    auto read_u8 = [&]() -> uint8_t {
        uint8_t v = ptr[offset];
        offset += 1;
        return v;
    };

    if (memcmp(ptr + offset, "CAFX", 4) != 0) {
        return false;
    }
    offset += 4;
    uint32_t version = read_u32();
    if (version != 1) {
        return false;
    }
    int32_t saved_width = read_i32();
    int32_t saved_height = read_i32();
    if (saved_width != width || saved_height != height) {
        return false;
    }
    uint32_t material_count = read_u32();

    std::vector<uint16_t> id_mapping(material_count, 0);
    for (uint32_t i = 0; i < material_count; i++) {
        uint16_t name_len = read_u16();
        if (offset + name_len > static_cast<size_t>(p_data.size())) {
            return false;
        }
        std::string name(reinterpret_cast<const char *>(ptr + offset), name_len);
        offset += name_len;
        id_mapping[i] = registry.get_material_id(name);
    }

    size_t expected_cell_size = static_cast<size_t>(width * height) * 5;
    if (offset + expected_cell_size > static_cast<size_t>(p_data.size())) {
        return false;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t saved_id = read_u16();
            int16_t temp = read_i16();
            uint8_t flags = read_u8();
            uint16_t current_id = saved_id < id_mapping.size() ? id_mapping[saved_id] : 0;
            size_t base = static_cast<size_t>(cell_index(x, y)) * 2;
            cpu_grid[base + 0] = static_cast<uint32_t>(current_id) | (static_cast<uint32_t>(static_cast<uint16_t>(temp)) << 16u);
            cpu_grid[base + 1] = static_cast<uint32_t>(flags);
        }
    }

    gpu_grid_dirty = true;
    cpu_grid_dirty = false;
    return true;
}

} // namespace ca
