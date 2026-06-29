#ifndef CELLULAR_AUTOMATA_CELL_H
#define CELLULAR_AUTOMATA_CELL_H

#include <cstdint>

namespace ca {

struct Cell {
    uint16_t material_id = 0;
    uint8_t flags = 0;

    static constexpr uint8_t FLAG_MOVED = 1 << 0;
    static constexpr uint8_t FLAG_SLEEP = 1 << 1;

    bool moved() const { return flags & FLAG_MOVED; }
    void set_moved(bool p_moved) {
        if (p_moved) flags |= FLAG_MOVED;
        else flags &= ~FLAG_MOVED;
    }

    bool sleeping() const { return flags & FLAG_SLEEP; }
    void set_sleep(bool p_sleep) {
        if (p_sleep) flags |= FLAG_SLEEP;
        else flags &= ~FLAG_SLEEP;
    }

    // Bits 2-7 store a per-cell lifetime counter (0-63).
    uint8_t get_lifetime() const { return flags >> 2; }
    void set_lifetime(uint8_t p_life) { flags = (flags & 0x03) | ((p_life & 0x3F) << 2); }
};

} // namespace ca

#endif // CELLULAR_AUTOMATA_CELL_H
