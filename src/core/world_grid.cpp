#include "world_grid.h"

namespace ca {

WorldGrid::WorldGrid(int p_width, int p_height) {
    resize(p_width, p_height);
}

void WorldGrid::resize(int p_width, int p_height) {
    width = p_width;
    height = p_height;
    size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
    current.resize(count);
    next.resize(count);
    clear();
}

void WorldGrid::clear() {
    for (auto &c : current) {
        c.material_id = 0;
        c.flags = 0;
    }
    for (auto &c : next) {
        c.material_id = 0;
        c.flags = 0;
    }
}

void WorldGrid::swap_buffers() {
    current.swap(next);
}

} // namespace ca
