#include "cpu_simulator.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <chrono>
#include <algorithm>
#include <random>
#include <cstring>

namespace ca {

CPUSimulator::CPUSimulator() {
    unsigned seed = static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count());
    rng.seed(seed);
    int hc = static_cast<int>(std::thread::hardware_concurrency());
    thread_count = (hc > 0) ? hc : 4;
    if (thread_count > 1) {
        thread_count = std::max(2, thread_count - 1); // leave one core for the game thread
    }
}

CPUSimulator::~CPUSimulator() {
    shutdown();
}

bool CPUSimulator::initialize(int p_width, int p_height, MaterialRegistry &p_registry) {
    width = p_width;
    height = p_height;
    registry = &p_registry;
    grid.resize(width, height);
    image = godot::Image::create(width, height, false, godot::Image::FORMAT_RGBA8);
    image->fill(godot::Color(0, 0, 0, 0));
    reset_active_region();
    thread_pool = std::make_unique<ThreadPool>(static_cast<size_t>(thread_count));
    return true;
}

void CPUSimulator::reset_active_region() {
    active_region_valid = false;
    active_min_x = 0;
    active_max_x = 0;
    active_min_y = 0;
    active_max_y = 0;
}

void CPUSimulator::expand_active_region(int p_x, int p_y) {
    if (!active_region_valid) {
        active_min_x = active_max_x = p_x;
        active_min_y = active_max_y = p_y;
        active_region_valid = true;
    } else {
        active_min_x = std::min(active_min_x, p_x);
        active_max_x = std::max(active_max_x, p_x);
        active_min_y = std::min(active_min_y, p_y);
        active_max_y = std::max(active_max_y, p_y);
    }
}

void CPUSimulator::include_region(Region &r, int p_x, int p_y) {
    if (!r.valid) {
        r.min_x = r.max_x = p_x;
        r.min_y = r.max_y = p_y;
        r.valid = true;
    } else {
        r.min_x = std::min(r.min_x, p_x);
        r.max_x = std::max(r.max_x, p_x);
        r.min_y = std::min(r.min_y, p_y);
        r.max_y = std::max(r.max_y, p_y);
    }
}

void CPUSimulator::merge_region(Region &r_dst, const Region &r_src) {
    if (!r_src.valid) {
        return;
    }
    if (!r_dst.valid) {
        r_dst = r_src;
    } else {
        r_dst.min_x = std::min(r_dst.min_x, r_src.min_x);
        r_dst.max_x = std::max(r_dst.max_x, r_src.max_x);
        r_dst.min_y = std::min(r_dst.min_y, r_src.min_y);
        r_dst.max_y = std::max(r_dst.max_y, r_src.max_y);
    }
}

bool CPUSimulator::can_move_to(int p_x, int p_y, uint16_t p_current_mat) const {
    if (p_x < 0 || p_x >= width || p_y < 0 || p_y >= height) {
        return false;
    }
    const Cell &target = grid.cell_current(p_x, p_y);
    if (target.material_id == 0) {
        return true;
    }
    if (target.material_id == p_current_mat) {
        return false;
    }
    const Material *target_mat = registry->get_material(target.material_id);
    const Material *current_mat = registry->get_material(p_current_mat);
    if (current_mat->type == MaterialType::LIQUID && target_mat->type == MaterialType::LIQUID) {
        return current_mat->density > target_mat->density;
    }
    return false;
}

void CPUSimulator::move_particle(int p_x, int p_y, int p_tx, int p_ty) {
    Cell cell = grid.cell_current(p_x, p_y);
    cell.set_moved(true);
    cell.set_sleep(false);
    grid.cell_next(p_tx, p_ty) = cell;
    grid.cell_next(p_x, p_y).material_id = 0;
    grid.cell_next(p_x, p_y).flags = 0;
}

uint16_t CPUSimulator::resolve_material_name(const std::string &p_name) const {
    if (p_name.empty()) {
        return 0;
    }
    const Material *m = registry->get_material(p_name);
    return m ? m->id : 0;
}

bool CPUSimulator::is_hot_neighbor(int p_x, int p_y) const {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            int nx = p_x + dx;
            int ny = p_y + dy;
            if (!grid.in_bounds(nx, ny)) {
                continue;
            }
            uint16_t nid = grid.cell_current(nx, ny).material_id;
            const Material *nm = registry->get_material(nid);
            if (nm->type == MaterialType::GAS && nm->lifetime > 0) {
                return true;
            }
        }
    }
    return false;
}

void CPUSimulator::apply_reactions(int p_x, int p_y) {
    Cell &cell = grid.cell_next(p_x, p_y);
    uint16_t mat_id = cell.material_id;
    if (mat_id == 0) {
        return;
    }
    const Material *mat = registry->get_material(mat_id);

    // Explosion: explosive material adjacent to fire/heat detonates.
    if (mat->explosive && !mat->explode_to.empty() && is_hot_neighbor(p_x, p_y)) {
        uint16_t explode_id = resolve_material_name(mat->explode_to);
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = p_x + dx;
                int ny = p_y + dy;
                if (!grid.in_bounds(nx, ny)) {
                    continue;
                }
                Cell &neighbor = grid.cell_next(nx, ny);
                const Material *nm = registry->get_material(neighbor.material_id);
                if (nm != nullptr && nm->explosive) {
                    continue; // let the other explosive detonate on its own
                }
                neighbor.material_id = explode_id;
                neighbor.flags = 0;
            }
        }
        return;
    }

    // Burning: flammable material adjacent to fire turns into burn_to.
    if (mat->flammable && !mat->burn_to.empty() && is_hot_neighbor(p_x, p_y)) {
        cell.material_id = resolve_material_name(mat->burn_to);
        cell.flags = 0;
        return;
    }

    // Corrosion: corrosive material eats a neighbor. Liquids prefer eating downward.
    if (mat->corrosive) {
        int dirs[4][2];
        if (mat->type == MaterialType::LIQUID) {
            int order[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
            std::copy(&order[0][0], &order[0][0] + 8, &dirs[0][0]);
        } else {
            int order[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            std::copy(&order[0][0], &order[0][0] + 8, &dirs[0][0]);
        }
        std::shuffle(std::begin(dirs), std::end(dirs), rng);
        for (auto &d : dirs) {
            int nx = p_x + d[0];
            int ny = p_y + d[1];
            if (!grid.in_bounds(nx, ny)) {
                continue;
            }
            Cell &neighbor = grid.cell_next(nx, ny);
            if (neighbor.material_id == 0 || neighbor.material_id == mat_id) {
                continue;
            }
            const Material *nm = registry->get_material(neighbor.material_id);
            if (nm->corrosive) {
                continue; // corrosives do not eat each other
            }
            float chance = static_cast<float>(rng() % 10000) / 10000.0f;
            if (chance < mat->corrosion_chance) {
                uint16_t residue_id = resolve_material_name(mat->corrosion_residue);
                neighbor.material_id = residue_id;
                neighbor.flags = 0;
            }
            break; // only try one neighbor per frame
        }
    }

    // Lifetime decay.
    if (mat->lifetime > 0) {
        uint8_t life = cell.get_lifetime();
        if (life == 0) {
            life = static_cast<uint8_t>(std::min(mat->lifetime, static_cast<uint16_t>(63)));
        }
        if (life <= 1) {
            uint16_t decay_id = resolve_material_name(mat->decay_to);
            cell.material_id = decay_id;
            cell.flags = 0;
        } else {
            cell.set_lifetime(life - 1);
        }
    }
}

void CPUSimulator::thermal_init_band(int p_min_x, int p_max_x, int p_min_y, int p_max_y) {
    for (int y = p_min_y; y <= p_max_y; y++) {
        for (int x = p_min_x; x <= p_max_x; x++) {
            Cell &next = grid.cell_next(x, y);
            if (next.material_id != 0) {
                next.temperature = grid.cell_current(x, y).temperature;
            }
        }
    }
}

void CPUSimulator::thermal_compute_band(int p_min_x, int p_max_x, int p_min_y, int p_max_y) {
    int band_w = p_max_x - p_min_x + 1;
    int band_h = p_max_y - p_min_y + 1;
    std::vector<int16_t> new_temperatures(static_cast<size_t>(band_w * band_h));

    // Heat diffusion using 4-neighbor average.
    for (int y = p_min_y; y <= p_max_y; y++) {
        for (int x = p_min_x; x <= p_max_x; x++) {
            Cell &next = grid.cell_next(x, y);
            if (next.material_id == 0) {
                continue;
            }
            const Material *mat = registry->get_material(next.material_id);
            int conductivity = std::clamp(mat->thermal_conductivity, 0, 100);
            int32_t sum = next.temperature;
            int count = 1;
            const int dxs[4] = {-1, 1, 0, 0};
            const int dys[4] = {0, 0, -1, 1};
            for (int i = 0; i < 4; i++) {
                int nx = x + dxs[i];
                int ny = y + dys[i];
                if (!grid.in_bounds(nx, ny)) {
                    continue;
                }
                const Cell &neighbor = grid.cell_next(nx, ny);
                if (neighbor.material_id == 0) {
                    continue;
                }
                sum += neighbor.temperature;
                count++;
            }
            int32_t avg = sum / count;
            int32_t diff = avg - next.temperature;
            int32_t change = (diff * conductivity) / 100;
            int idx = (y - p_min_y) * band_w + (x - p_min_x);
            new_temperatures[idx] = static_cast<int16_t>(std::clamp(next.temperature + change, static_cast<int32_t>(-32768), static_cast<int32_t>(32767)));
        }
    }

    // Apply new temperatures and resolve phase changes.
    for (int y = p_min_y; y <= p_max_y; y++) {
        for (int x = p_min_x; x <= p_max_x; x++) {
            Cell &next = grid.cell_next(x, y);
            if (next.material_id == 0) {
                continue;
            }
            int idx = (y - p_min_y) * band_w + (x - p_min_x);
            next.temperature = new_temperatures[idx];

            const Material *mat = registry->get_material(next.material_id);
            uint16_t new_mat_id = next.material_id;
            if (next.temperature > mat->boiling_point && !mat->gas_form.empty()) {
                new_mat_id = resolve_material_name(mat->gas_form);
            } else if (next.temperature < mat->freeze_point && !mat->solid_form.empty()) {
                new_mat_id = resolve_material_name(mat->solid_form);
            } else if (next.temperature > mat->melting_point && !mat->liquid_form.empty()) {
                new_mat_id = resolve_material_name(mat->liquid_form);
            }
            if (new_mat_id != next.material_id && new_mat_id != 0) {
                next.material_id = new_mat_id;
                next.flags = 0;
                const Material *new_mat = registry->get_material(new_mat_id);
                if (new_mat != nullptr) {
                    next.temperature = static_cast<int16_t>(new_mat->temperature);
                }
            }
        }
    }
}

void CPUSimulator::update() {
    frame_counter++;
    const bool wake_sleeping = (frame_counter % 10u) == 0u;

    if (!active_region_valid) {
        grid.swap_buffers();
        return;
    }

    // Expand the active region to account for particles that may fall/move into neighboring cells.
    int min_x = std::max(0, active_min_x - 2);
    int max_x = std::min(width - 1, active_max_x + 2);
    int min_y = std::max(0, active_min_y - 2); // gases may rise
    int max_y = std::min(height - 1, active_max_y + 2);

    // Clear only the expanded active region in the next buffer.
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            Cell &c = grid.cell_next(x, y);
            c.material_id = 0;
            c.flags = 0;
        }
    }

    bool new_region_valid = false;
    int new_min_x = 0, new_max_x = 0, new_min_y = 0, new_max_y = 0;
    auto include_in_new_region = [&](int x, int y) {
        if (!new_region_valid) {
            new_min_x = new_max_x = x;
            new_min_y = new_max_y = y;
            new_region_valid = true;
        } else {
            new_min_x = std::min(new_min_x, x);
            new_max_x = std::max(new_max_x, x);
            new_min_y = std::min(new_min_y, y);
            new_max_y = std::max(new_max_y, y);
        }
    };

    // Process from bottom to top to let gravity work in one pass.
    for (int y = max_y; y >= min_y; y--) {
        for (int x = min_x; x <= max_x; x++) {
            const Cell &cell = grid.cell_current(x, y);
            uint16_t mat_id = cell.material_id;
            if (mat_id == 0) {
                continue;
            }

            const Material *mat = registry->get_material(mat_id);

            // Sleeping movable particles are skipped most frames and woken up periodically.
            if (!wake_sleeping && cell.sleeping() && mat->type != MaterialType::SOLID) {
                grid.cell_next(x, y) = cell;
                include_in_new_region(x, y);
                continue;
            }

            int tx = x;
            int ty = y;

            if (mat->type == MaterialType::SOLID) {
                // Solids do not move.
                grid.cell_next(x, y) = cell;
                include_in_new_region(x, y);
                apply_reactions(x, y);
                continue;
            }

            if (mat->type == MaterialType::GAS) {
                // Gases rise straight up or diagonally.
                if (can_move_to(x, y - 1, mat_id)) {
                    ty = y - 1;
                } else {
                    bool prefer_left = (rng() % 2) == 0;
                    if (prefer_left) {
                        if (can_move_to(x - 1, y - 1, mat_id)) {
                            tx = x - 1;
                            ty = y - 1;
                        } else if (can_move_to(x + 1, y - 1, mat_id)) {
                            tx = x + 1;
                            ty = y - 1;
                        }
                    } else {
                        if (can_move_to(x + 1, y - 1, mat_id)) {
                            tx = x + 1;
                            ty = y - 1;
                        } else if (can_move_to(x - 1, y - 1, mat_id)) {
                            tx = x - 1;
                            ty = y - 1;
                        }
                    }
                }
            } else if (mat->type == MaterialType::POWDER || mat->type == MaterialType::LIQUID) {
                // Try fall straight down.
                if (can_move_to(x, y + 1, mat_id)) {
                    ty = y + 1;
                }
                // Try diagonal fall.
                else {
                    bool prefer_left = (rng() % 2) == 0;
                    if (prefer_left) {
                        if (can_move_to(x - 1, y + 1, mat_id)) {
                            tx = x - 1;
                            ty = y + 1;
                        } else if (can_move_to(x + 1, y + 1, mat_id)) {
                            tx = x + 1;
                            ty = y + 1;
                        }
                    } else {
                        if (can_move_to(x + 1, y + 1, mat_id)) {
                            tx = x + 1;
                            ty = y + 1;
                        } else if (can_move_to(x - 1, y + 1, mat_id)) {
                            tx = x - 1;
                            ty = y + 1;
                        }
                    }

                    // For liquids, also try horizontal spread.
                    if (mat->type == MaterialType::LIQUID && tx == x && ty == y) {
                        bool prefer_left_h = (rng() % 2) == 0;
                        if (prefer_left_h) {
                            if (can_move_to(x - 1, y, mat_id)) {
                                tx = x - 1;
                            } else if (can_move_to(x + 1, y, mat_id)) {
                                tx = x + 1;
                            }
                        } else {
                            if (can_move_to(x + 1, y, mat_id)) {
                                tx = x + 1;
                            } else if (can_move_to(x - 1, y, mat_id)) {
                                tx = x - 1;
                            }
                        }
                    }
                }
            }

            bool moved = false;
            if (tx == x && ty == y) {
                // Stay in place; particle may go to sleep if it keeps resting.
                Cell resting = cell;
                resting.set_sleep(true);
                grid.cell_next(x, y) = resting;
            } else if (grid.cell_next(tx, ty).material_id == 0) {
                move_particle(x, y, tx, ty);
                moved = true;
            } else {
                // Target already occupied (race in same frame), stay.
                Cell resting = cell;
                resting.set_sleep(true);
                grid.cell_next(x, y) = resting;
            }

            include_in_new_region(x, y);
            include_in_new_region(tx, ty);
            apply_reactions(moved ? tx : x, moved ? ty : y);
        }
    }

    // Heat diffusion and phase change over the next buffer, split into bands
    // and processed in parallel. Initialization must finish before any band
    // reads neighbor temperatures.
    int thermal_bands = std::min(thread_count, std::max(1, max_y - min_y + 1));
    for (int b = 0; b < thermal_bands; b++) {
        int y0 = min_y + b * (max_y - min_y + 1) / thermal_bands;
        int y1 = min_y + (b + 1) * (max_y - min_y + 1) / thermal_bands - 1;
        thread_pool->enqueue([&, y0, y1]() {
            thermal_init_band(min_x, max_x, y0, y1);
        });
    }
    thread_pool->wait_all();

    for (int b = 0; b < thermal_bands; b++) {
        int y0 = min_y + b * (max_y - min_y + 1) / thermal_bands;
        int y1 = min_y + (b + 1) * (max_y - min_y + 1) / thermal_bands - 1;
        thread_pool->enqueue([&, y0, y1]() {
            thermal_compute_band(min_x, max_x, y0, y1);
        });
    }
    thread_pool->wait_all();

    grid.swap_buffers();

    active_region_valid = new_region_valid;
    if (new_region_valid) {
        active_min_x = new_min_x;
        active_max_x = new_max_x;
        active_min_y = new_min_y;
        active_max_y = new_max_y;
    }
}

void CPUSimulator::set_cell(int p_x, int p_y, uint16_t p_material_id) {
    if (!grid.in_bounds(p_x, p_y)) {
        return;
    }
    grid.cell_current(p_x, p_y).material_id = p_material_id;
    grid.cell_current(p_x, p_y).flags = 0;
    const Material *mat = registry->get_material(p_material_id);
    grid.cell_current(p_x, p_y).temperature = mat ? static_cast<int16_t>(mat->temperature) : static_cast<int16_t>(0);
    if (p_material_id != 0) {
        expand_active_region(p_x, p_y);
    }
}

uint16_t CPUSimulator::get_cell(int p_x, int p_y) const {
    if (!grid.in_bounds(p_x, p_y)) {
        return 0;
    }
    return grid.cell_current(p_x, p_y).material_id;
}

godot::Ref<godot::Image> CPUSimulator::render_image() {
    image->fill(godot::Color(0, 0, 0, 0));

    const std::vector<Material> &materials = registry->get_all();
    int w = image->get_width();
    int h = image->get_height();

    // Render scanlines in parallel; each thread writes disjoint rows of the
    // output image, so there is no contention.
    int bands = std::min(thread_count, std::max(1, h));
    for (int b = 0; b < bands; b++) {
        int y0 = b * h / bands;
        int y1 = (b + 1) * h / bands;
        thread_pool->enqueue([&, y0, y1]() {
            for (int y = y0; y < y1; y++) {
                for (int x = 0; x < w; x++) {
                    uint16_t mat_id = grid.cell_current(x, y).material_id;
                    if (mat_id == 0) {
                        continue;
                    }
                    if (mat_id < materials.size()) {
                        const Material &mat = materials[mat_id];
                        godot::Color c = mat.color;
                        int temp_diff = static_cast<int>(grid.cell_current(x, y).temperature) - mat.temperature;
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
                            // Emissive materials already use their glow color; still blend slightly.
                            image->set_pixel(x, y, mat.glow_color.lerp(c, 0.3f));
                        } else {
                            image->set_pixel(x, y, c);
                        }
                    }
                }
            }
        });
    }
    thread_pool->wait_all();

    return image;
}

int CPUSimulator::get_particle_count() const {
    int bands = std::min(thread_count, std::max(1, height));
    std::vector<int> counts(static_cast<size_t>(bands), 0);
    for (int b = 0; b < bands; b++) {
        int y0 = b * height / bands;
        int y1 = (b + 1) * height / bands;
        thread_pool->enqueue([&, y0, y1, b]() {
            int c = 0;
            for (int y = y0; y < y1; y++) {
                for (int x = 0; x < width; x++) {
                    if (grid.cell_current(x, y).material_id != 0) {
                        c++;
                    }
                }
            }
            counts[b] = c;
        });
    }
    thread_pool->wait_all();

    int total = 0;
    for (int c : counts) {
        total += c;
    }
    return total;
}

void CPUSimulator::clear() {
    grid.clear();
    reset_active_region();
}

void CPUSimulator::shutdown() {
    image.unref();
    thread_pool.reset();
    reset_active_region();
    registry = nullptr;
}

godot::PackedByteArray CPUSimulator::serialize() const {
    godot::PackedByteArray data;
    const std::vector<Material> &materials = registry->get_all();

    // Header: magic, version, width, height, material count.
    size_t header_size = 4 + 4 + 4 + 4 + 4;
    size_t material_table_size = 0;
    for (const Material &m : materials) {
        material_table_size += 2 + m.name.size();
    }
    size_t cell_size = 2 + 2 + 1; // material_id + temperature + flags
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
    write_u32(1); // version
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
            const Cell &cell = grid.cell_current(x, y);
            write_u16(cell.material_id);
            write_i16(cell.temperature);
            write_u8(cell.flags);
        }
    }

    return data;
}

bool CPUSimulator::deserialize(const godot::PackedByteArray &p_data) {
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
        id_mapping[i] = registry->get_material_id(name);
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
            Cell &cell = grid.cell_current(x, y);
            cell.material_id = current_id;
            cell.temperature = temp;
            cell.flags = flags;
        }
    }

    // Recompute active region.
    reset_active_region();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (grid.cell_current(x, y).material_id != 0) {
                expand_active_region(x, y);
            }
        }
    }

    return true;
}

} // namespace ca
