#[compute]
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict readonly buffer SrcGrid {
    uvec2 cells[];
} src;

layout(set = 0, binding = 1, std430) restrict buffer DstGrid {
    uvec2 cells[];
} dst;

layout(set = 0, binding = 2, std430) restrict readonly buffer MaterialBuffer {
    uvec4 data[];
} materials;

layout(push_constant, std430) uniform Params {
    int width;
    int height;
    int pass_id;
    int frame_seed;
} params;

const uint TYPE_SOLID = 0u;
const uint TYPE_POWDER = 1u;
const uint TYPE_LIQUID = 2u;
const uint TYPE_GAS = 3u;

const uint FLAG_MOVED = 1u << 0u;
const uint FLAG_SLEEP = 1u << 1u;
const uint LIFETIME_SHIFT = 2u;
const uint LIFETIME_MASK = 0x3Fu;

uint get_material_count() {
    return materials.data[0].x;
}

uvec4 get_material_packed(uint mat_id) {
    if (mat_id == 0u || mat_id >= get_material_count()) return uvec4(0u);
    return materials.data[1u + mat_id * 3u];
}

uvec4 get_material_extra(uint mat_id) {
    if (mat_id == 0u || mat_id >= get_material_count()) return uvec4(0u);
    return materials.data[2u + mat_id * 3u];
}

uvec4 get_material_phase(uint mat_id) {
    if (mat_id == 0u || mat_id >= get_material_count()) return uvec4(0u);
    return materials.data[3u + mat_id * 3u];
}

uint get_type(uint mat_id) {
    return get_material_packed(mat_id).x & 0xFFu;
}

uint get_density(uint mat_id) {
    return (get_material_packed(mat_id).x >> 8) & 0xFFu;
}

uint get_lifetime(uint mat_id) {
    return (get_material_packed(mat_id).x >> 16) & 0xFFFFu;
}

bool is_flammable(uint mat_id) {
    return (get_material_packed(mat_id).w & 1u) != 0u;
}

bool is_corrosive(uint mat_id) {
    return (get_material_packed(mat_id).w & 2u) != 0u;
}

bool is_explosive(uint mat_id) {
    return (get_material_packed(mat_id).w & 4u) != 0u;
}

uint get_burn_to(uint mat_id) {
    return get_material_packed(mat_id).y & 0xFFFFu;
}

uint get_explode_to(uint mat_id) {
    return (get_material_packed(mat_id).y >> 16) & 0xFFFFu;
}

uint get_decay_to(uint mat_id) {
    return get_material_packed(mat_id).z & 0xFFFFu;
}

uint get_residue(uint mat_id) {
    return (get_material_packed(mat_id).z >> 16) & 0xFFFFu;
}

uint get_corrosion_chance_u8(uint mat_id) {
    return (get_material_packed(mat_id).w >> 8) & 0xFFu;
}

int get_default_temperature(uint mat_id) {
    return int(get_material_extra(mat_id).x);
}

uint get_glow_color(uint mat_id) {
    return get_material_extra(mat_id).y;
}

bool get_emit_light(uint mat_id) {
    return get_material_extra(mat_id).z != 0u;
}

int get_melting_point(uint mat_id) {
    uint raw = get_material_phase(mat_id).x & 0xFFFFu;
    return (raw >= 32768u) ? int(raw) - 65536 : int(raw);
}

int get_boiling_point(uint mat_id) {
    uint raw = (get_material_phase(mat_id).x >> 16) & 0xFFFFu;
    return (raw >= 32768u) ? int(raw) - 65536 : int(raw);
}

int get_freeze_point(uint mat_id) {
    uint raw = get_material_phase(mat_id).y & 0xFFFFu;
    return (raw >= 32768u) ? int(raw) - 65536 : int(raw);
}

uint get_thermal_conductivity(uint mat_id) {
    return (get_material_phase(mat_id).y >> 16) & 0xFFFFu;
}

uint get_solid_form(uint mat_id) {
    return get_material_phase(mat_id).z & 0xFFFFu;
}

uint get_liquid_form(uint mat_id) {
    return (get_material_phase(mat_id).z >> 16) & 0xFFFFu;
}

uint get_gas_form(uint mat_id) {
    return get_material_phase(mat_id).w & 0xFFFFu;
}

bool is_empty(uint mat_id) { return mat_id == 0u; }
bool is_liquid(uint mat_id) { return get_type(mat_id) == TYPE_LIQUID; }
bool is_powder(uint mat_id) { return get_type(mat_id) == TYPE_POWDER; }
bool is_gas(uint mat_id) { return get_type(mat_id) == TYPE_GAS; }
bool is_solid(uint mat_id) { return get_type(mat_id) == TYPE_SOLID; }
bool is_movable(uint mat_id) {
    uint t = get_type(mat_id);
    return t == TYPE_POWDER || t == TYPE_LIQUID;
}
bool is_hot(uint mat_id) {
    return is_gas(mat_id) && get_lifetime(mat_id) > 0u;
}

int cell_index(int x, int y) {
    return y * params.width + x;
}

uvec2 make_cell(uint mat_id, int temp, uint flags) {
    uint t_raw = uint(temp);
    if (temp < 0) t_raw = uint(temp + 65536);
    return uvec2((mat_id & 0xFFFFu) | (t_raw << 16u), flags & 0xFFu);
}

uint get_mat_id(uvec2 cell) {
    return cell.x & 0xFFFFu;
}

int get_temperature(uvec2 cell) {
    uint raw = cell.x >> 16;
    return (raw >= 32768u) ? int(raw) - 65536 : int(raw);
}

uint get_flags(uvec2 cell) {
    return cell.y & 0xFFu;
}

uvec2 set_flags(uvec2 cell, uint flags) {
    cell.y = (cell.y & ~0xFFu) | (flags & 0xFFu);
    return cell;
}

uvec2 set_temperature(uvec2 cell, int temp) {
    uint t_raw = uint(temp);
    if (temp < 0) t_raw = uint(temp + 65536);
    cell.x = (cell.x & 0xFFFFu) | (t_raw << 16u);
    return cell;
}

uvec2 set_mat_id(uvec2 cell, uint mat_id) {
    cell.x = (cell.x & ~0xFFFFu) | (mat_id & 0xFFFFu);
    return cell;
}

uvec2 src_at(int x, int y) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return uvec2(0u);
    return src.cells[cell_index(x, y)];
}

uvec2 dst_at(int x, int y) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return uvec2(0u);
    return dst.cells[cell_index(x, y)];
}

void clear_dst(int x, int y) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return;
    dst.cells[cell_index(x, y)] = uvec2(0u);
}

void store_dst(int x, int y, uvec2 cell) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return;
    dst.cells[cell_index(x, y)] = cell;
}

void pull_from(int dst_x, int dst_y, int src_x, int src_y) {
    uvec2 cell = src_at(src_x, src_y);
    // Mark moved, clear sleep.
    uint flags = get_flags(cell);
    flags |= FLAG_MOVED;
    flags &= ~FLAG_SLEEP;
    cell = set_flags(cell, flags);
    store_dst(dst_x, dst_y, cell);
    clear_dst(src_x, src_y);
}

bool can_displace(uint src_id, uint dst_id) {
    if (is_empty(dst_id)) return true;
    if (src_id == dst_id) return false;
    if (is_liquid(src_id) && is_liquid(dst_id)) {
        return get_density(src_id) > get_density(dst_id);
    }
    return false;
}

bool has_hot_neighbor(int x, int y) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            if (is_hot(get_mat_id(src_at(x + dx, y + dy)))) return true;
        }
    }
    return false;
}

// Simple 2D hash for deterministic pseudo-randomness based on frame seed and position.
uint hash2(int x, int y) {
    uint h = uint(params.frame_seed) * 73856093u;
    h ^= uint(x) * 19349663u;
    h ^= uint(y) * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void main() {
    int idx = int(gl_GlobalInvocationID.x);
    int total = params.width * params.height;
    if (idx >= total) return;

    int x = idx % params.width;
    int y = idx / params.width;

    uvec2 self = src_at(x, y);
    uint self_id = get_mat_id(self);

    // Movement passes operate on empty destination cells (pull model).
    if (params.pass_id <= 2) {
        // Default: copy source to destination.
        store_dst(x, y, self);

        if (!is_empty(self_id)) {
            return;
        }

        if (params.pass_id == 0) {
            // Vertical fall for powder/liquid.
            uint above_id = get_mat_id(src_at(x, y - 1));
            if (is_movable(above_id) && can_displace(above_id, self_id)) {
                pull_from(x, y, x, y - 1);
            } else {
                // Vertical rise for gas.
                uint below_id = get_mat_id(src_at(x, y + 1));
                if (is_gas(below_id)) {
                    pull_from(x, y, x, y + 1);
                }
            }
        } else if (params.pass_id == 1) {
            // Diagonal fall.
            uint above_left = get_mat_id(src_at(x - 1, y - 1));
            uint left = get_mat_id(src_at(x - 1, y));
            bool can_left = is_movable(above_left) && !is_empty(left) && can_displace(above_left, self_id);

            uint above_right = get_mat_id(src_at(x + 1, y - 1));
            uint right = get_mat_id(src_at(x + 1, y));
            bool can_right = is_movable(above_right) && !is_empty(right) && can_displace(above_right, self_id);

            if (can_left && can_right) {
                bool prefer_left = ((x + y + params.frame_seed) & 1) == 0;
                if (prefer_left) {
                    pull_from(x, y, x - 1, y - 1);
                } else {
                    pull_from(x, y, x + 1, y - 1);
                }
            } else if (can_left) {
                pull_from(x, y, x - 1, y - 1);
            } else if (can_right) {
                pull_from(x, y, x + 1, y - 1);
            } else {
                // Diagonal rise for gas.
                uint below_left = get_mat_id(src_at(x - 1, y + 1));
                uint bl = get_mat_id(src_at(x - 1, y));
                bool can_rise_left = is_gas(below_left) && !is_gas(bl);

                uint below_right = get_mat_id(src_at(x + 1, y + 1));
                uint br = get_mat_id(src_at(x + 1, y));
                bool can_rise_right = is_gas(below_right) && !is_gas(br);

                if (can_rise_left && can_rise_right) {
                    bool prefer_left = ((x + y + params.frame_seed) & 1) == 0;
                    if (prefer_left) {
                        pull_from(x, y, x - 1, y + 1);
                    } else {
                        pull_from(x, y, x + 1, y + 1);
                    }
                } else if (can_rise_left) {
                    pull_from(x, y, x - 1, y + 1);
                } else if (can_rise_right) {
                    pull_from(x, y, x + 1, y + 1);
                }
            }
        } else if (params.pass_id == 2) {
            // Horizontal flow for liquids and gases.
            uint left_id = get_mat_id(src_at(x - 1, y));
            uint right_id = get_mat_id(src_at(x + 1, y));
            bool can_left = (is_liquid(left_id) || is_gas(left_id)) && can_displace(left_id, self_id);
            bool can_right = (is_liquid(right_id) || is_gas(right_id)) && can_displace(right_id, self_id);

            if (can_left && can_right) {
                bool prefer_left = ((x + params.frame_seed) & 1) == 0;
                if (prefer_left) {
                    pull_from(x, y, x - 1, y);
                } else {
                    pull_from(x, y, x + 1, y);
                }
            } else if (can_left) {
                pull_from(x, y, x - 1, y);
            } else if (can_right) {
                pull_from(x, y, x + 1, y);
            }
        }
        return;
    }

    // Pass 3: reactions, lifetime decay, heat diffusion and phase changes.
    // Work on the post-movement destination buffer.
    uvec2 cell = dst_at(x, y);
    uint mat_id = get_mat_id(cell);
    if (is_empty(mat_id)) {
        return;
    }

    int temp = get_temperature(cell);
    uint flags = get_flags(cell);

    // Reactions.
    bool reacted = false;

    // Explosion.
    if (is_explosive(mat_id) && !reacted) {
        uint explode_to = get_explode_to(mat_id);
        if (explode_to != 0u && has_hot_neighbor(x, y)) {
            cell = set_mat_id(cell, explode_to);
            temp = get_default_temperature(explode_to);
            flags = 0u;
            reacted = true;
        }
    }

    // Burning.
    if (is_flammable(mat_id) && !reacted) {
        uint burn_to = get_burn_to(mat_id);
        if (burn_to != 0u && has_hot_neighbor(x, y)) {
            cell = set_mat_id(cell, burn_to);
            temp = get_default_temperature(burn_to);
            flags = 0u;
            reacted = true;
        }
    }

    // Lifetime decay.
    if (!reacted) {
        uint max_life = get_lifetime(mat_id);
        if (max_life > 0u) {
            uint life = (flags >> LIFETIME_SHIFT) & LIFETIME_MASK;
            if (life == 0u) {
                life = min(max_life, 63u);
            }
            if (life <= 1u) {
                uint decay_to = get_decay_to(mat_id);
                if (decay_to != 0u) {
                    cell = set_mat_id(cell, decay_to);
                    temp = get_default_temperature(decay_to);
                }
                flags = 0u;
            } else {
                life -= 1u;
                flags = (flags & ~(LIFETIME_MASK << LIFETIME_SHIFT)) | (life << LIFETIME_SHIFT);
            }
        }
    }

    // Heat diffusion using 4-neighbor average.
    mat_id = get_mat_id(cell);
    uint cond_u = get_thermal_conductivity(mat_id);
    if (cond_u > 0u && !is_empty(mat_id)) {
        int conductivity = int(min(cond_u, 100u));
        int sum = temp;
        int count = 1;
        const int dxs[4] = {-1, 1, 0, 0};
        const int dys[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nx = x + dxs[i];
            int ny = y + dys[i];
            uvec2 nb = dst_at(nx, ny);
            if (!is_empty(get_mat_id(nb))) {
                sum += get_temperature(nb);
                count++;
            }
        }
        int avg = sum / count;
        int diff = avg - temp;
        int change = (diff * conductivity) / 100;
        temp += change;
        temp = clamp(temp, -32768, 32767);
    }

    // Phase changes.
    mat_id = get_mat_id(cell);
    uint new_id = mat_id;
    if (temp > get_boiling_point(mat_id)) {
        new_id = get_gas_form(mat_id);
    } else if (temp < get_freeze_point(mat_id)) {
        new_id = get_solid_form(mat_id);
    } else if (temp > get_melting_point(mat_id)) {
        new_id = get_liquid_form(mat_id);
    }
    if (new_id != 0u && new_id != mat_id) {
        cell = set_mat_id(cell, new_id);
        temp = get_default_temperature(new_id);
        flags = 0u;
    }

    cell = set_temperature(cell, temp);
    cell = set_flags(cell, flags);
    store_dst(x, y, cell);
}
