#include "cpu_simulator.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <chrono>
#include <algorithm>
#include <random>

namespace ca {

CPUSimulator::CPUSimulator() {
    unsigned seed = static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count());
    rng.seed(seed);
}

CPUSimulator::~CPUSimulator() {
    shutdown();
}

void CPUSimulator::init(int p_width, int p_height) {
    width = p_width;
    height = p_height;
    grid.resize(width, height);
    image = godot::Image::create(width, height, false, godot::Image::FORMAT_RGBA8);
    image->fill(godot::Color(0, 0, 0, 0));
    reset_active_region();
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
    const Material *target_mat = registry.get_material(target.material_id);
    const Material *current_mat = registry.get_material(p_current_mat);
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
    const Material *m = registry.get_material(p_name);
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
            const Material *nm = registry.get_material(nid);
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
    const Material *mat = registry.get_material(mat_id);

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
                const Material *nm = registry.get_material(neighbor.material_id);
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
            const Material *nm = registry.get_material(neighbor.material_id);
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

            const Material *mat = registry.get_material(mat_id);

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

godot::Ref<godot::Image> CPUSimulator::get_image() {
    image->fill(godot::Color(0, 0, 0, 0));

    const std::vector<Material> &materials = registry.get_all();
    int w = image->get_width();
    int h = image->get_height();

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t mat_id = grid.cell_current(x, y).material_id;
            if (mat_id == 0) {
                continue;
            }
            if (mat_id < materials.size()) {
                const Material &mat = materials[mat_id];
                if (mat.emit_light) {
                    image->set_pixel(x, y, mat.glow_color);
                } else {
                    image->set_pixel(x, y, mat.color);
                }
            }
        }
    }

    return image;
}

int CPUSimulator::get_particle_count() const {
    int count = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (grid.cell_current(x, y).material_id != 0) {
                count++;
            }
        }
    }
    return count;
}

void CPUSimulator::clear() {
    grid.clear();
    reset_active_region();
}

void CPUSimulator::shutdown() {
    image.unref();
    reset_active_region();
}

} // namespace ca
