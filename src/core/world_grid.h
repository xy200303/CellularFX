#ifndef CELLULAR_AUTOMATA_WORLD_GRID_H
#define CELLULAR_AUTOMATA_WORLD_GRID_H

#include "cell.h"

#include <vector>
#include <cstdint>

namespace ca {

class WorldGrid {
private:
    int width = 0;
    int height = 0;
    std::vector<Cell> current;
    std::vector<Cell> next;

public:
    WorldGrid() = default;
    WorldGrid(int p_width, int p_height);

    void resize(int p_width, int p_height);
    void clear();
    void swap_buffers();

    int get_width() const { return width; }
    int get_height() const { return height; }
    int get_count() const { return width * height; }

    bool in_bounds(int p_x, int p_y) const {
        return p_x >= 0 && p_x < width && p_y >= 0 && p_y < height;
    }

    int index(int p_x, int p_y) const { return p_y * width + p_x; }

    Cell &cell(int p_x, int p_y) { return current[index(p_x, p_y)]; }
    const Cell &cell(int p_x, int p_y) const { return current[index(p_x, p_y)]; }

    Cell &cell_current(int p_x, int p_y) { return current[index(p_x, p_y)]; }
    const Cell &cell_current(int p_x, int p_y) const { return current[index(p_x, p_y)]; }

    Cell &cell_next(int p_x, int p_y) { return next[index(p_x, p_y)]; }
    const Cell &cell_next(int p_x, int p_y) const { return next[index(p_x, p_y)]; }

    const std::vector<Cell> &get_current() const { return current; }
    std::vector<Cell> &get_current() { return current; }
    std::vector<Cell> &get_next() { return next; }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_WORLD_GRID_H
