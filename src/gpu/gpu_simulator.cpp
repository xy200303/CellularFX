#include "gpu_simulator.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>

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

    material_data.resize(count);
    for (size_t i = 0; i < count; i++) {
        const Material *m = registry.get_material(static_cast<uint16_t>(i));
        uint32_t packed = static_cast<uint32_t>(m->type) & 0xFFu;
        packed |= (static_cast<uint32_t>(m->density) & 0xFFu) << 8;
        material_data[i] = packed;
    }

    // Pack data into the pre-allocated buffer region.
    uint32_t buffer_size = 4 + MAX_MATERIALS * 4;
    godot::PackedByteArray data;
    data.resize(static_cast<int32_t>(buffer_size));
    uint8_t *ptr = data.ptrw();
    memset(ptr, 0, buffer_size);
    *reinterpret_cast<uint32_t *>(ptr) = static_cast<uint32_t>(count);
    if (!material_data.empty()) {
        memcpy(ptr + 4, material_data.data(), material_data.size() * 4);
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
    materials_buf = rd->storage_buffer_create(4 + MAX_MATERIALS * 4);
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

    for (int pass = 0; pass < 3; pass++) {
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
                img->set_pixel(x, y, materials[mat_id].color);
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
