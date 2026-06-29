#include "cpu_simulator.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <chrono>
#include <algorithm>

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
    grid.cell_next(p_tx, p_ty) = cell;
    grid.cell_next(p_x, p_y).material_id = 0;
    grid.cell_next(p_x, p_y).flags = 0;
}

void CPUSimulator::update() {
    if (!active_region_valid) {
        grid.swap_buffers();
        return;
    }

    // Expand the active region to account for particles that may fall/move into neighboring cells.
    int min_x = std::max(0, active_min_x - 2);
    int max_x = std::min(width - 1, active_max_x + 2);
    int min_y = std::max(0, active_min_y - 1);
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
            if (mat->type == MaterialType::SOLID) {
                grid.cell_next(x, y) = cell;
                include_in_new_region(x, y);
                continue;
            }

            int tx = x;
            int ty = y;

            if (mat->type == MaterialType::POWDER || mat->type == MaterialType::LIQUID) {
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

            if (tx == x && ty == y) {
                // Stay in place.
                grid.cell_next(x, y) = cell;
            } else if (grid.cell_next(tx, ty).material_id == 0) {
                move_particle(x, y, tx, ty);
            } else {
                // Target already occupied (race in same frame), stay.
                grid.cell_next(x, y) = cell;
            }

            include_in_new_region(x, y);
            include_in_new_region(tx, ty);
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
                image->set_pixel(x, y, materials[mat_id].color);
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
