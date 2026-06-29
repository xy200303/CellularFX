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

layout(set = 0, binding = 3, std430) restrict buffer ClaimBuffer {
    uint data[];
} claim;

layout(set = 0, binding = 4, std430) restrict buffer MovedToBuffer {
    uint data[];
} moved_to;

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

void store_dst(int x, int y, uvec2 cell) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return;
    dst.cells[cell_index(x, y)] = cell;
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

uint hash2(int x, int y) {
    uint h = uint(params.frame_seed) * 73856093u;
    h ^= uint(x) * 19349663u;
    h ^= uint(y) * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Attempt to claim target position for this source cell.
bool try_claim(int src_idx, int target_idx) {
    if (target_idx < 0 || target_idx >= params.width * params.height) return false;
    uint expected = 0u;
    uint desired = uint(src_idx + 1);
    uint old = atomicCompSwap(claim.data[target_idx], expected, desired);
    if (old == 0u) {
        atomicExchange(moved_to.data[src_idx], uint(target_idx + 1));
        return true;
    }
    return false;
}

void main() {
    int idx = int(gl_GlobalInvocationID.x);
    int total = params.width * params.height;
    if (idx >= total) return;

    int x = idx % params.width;
    int y = idx / params.width;

    if (params.pass_id == 0) {
        // Push/claim pass: each source cell tries to move to its preferred target.
        uvec2 self = src.cells[idx];
        uint self_id = get_mat_id(self);
        if (is_empty(self_id) || is_solid(self_id)) {
            return;
        }

        int tx = x;
        int ty = y;

        if (is_gas(self_id)) {
            if (is_empty(get_mat_id(src_at(x, y - 1)))) {
                ty = y - 1;
            } else {
                bool prefer_left = ((x + y + params.frame_seed) & 1) == 0;
                if (prefer_left) {
                    if (is_empty(get_mat_id(src_at(x - 1, y - 1)))) { tx = x - 1; ty = y - 1; }
                    else if (is_empty(get_mat_id(src_at(x + 1, y - 1)))) { tx = x + 1; ty = y - 1; }
                } else {
                    if (is_empty(get_mat_id(src_at(x + 1, y - 1)))) { tx = x + 1; ty = y - 1; }
                    else if (is_empty(get_mat_id(src_at(x - 1, y - 1)))) { tx = x - 1; ty = y - 1; }
                }
            }
        } else if (is_powder(self_id) || is_liquid(self_id)) {
            uint below_id = get_mat_id(src_at(x, y + 1));
            if (can_displace(self_id, below_id)) {
                ty = y + 1;
            } else {
                bool prefer_left = ((x + y + params.frame_seed) & 1) == 0;
                if (prefer_left) {
                    uint dl_id = get_mat_id(src_at(x - 1, y + 1));
                    if (can_displace(self_id, dl_id)) { tx = x - 1; ty = y + 1; }
                    else {
                        uint dr_id = get_mat_id(src_at(x + 1, y + 1));
                        if (can_displace(self_id, dr_id)) { tx = x + 1; ty = y + 1; }
                    }
                } else {
                    uint dr_id = get_mat_id(src_at(x + 1, y + 1));
                    if (can_displace(self_id, dr_id)) { tx = x + 1; ty = y + 1; }
                    else {
                        uint dl_id = get_mat_id(src_at(x - 1, y + 1));
                        if (can_displace(self_id, dl_id)) { tx = x - 1; ty = y + 1; }
                    }
                }

                if (is_liquid(self_id) && tx == x && ty == y) {
                    bool prefer_left_h = ((x + params.frame_seed) & 1) == 0;
                    if (prefer_left_h) {
                        uint l_id = get_mat_id(src_at(x - 1, y));
                        if (can_displace(self_id, l_id)) tx = x - 1;
                        else {
                            uint r_id = get_mat_id(src_at(x + 1, y));
                            if (can_displace(self_id, r_id)) tx = x + 1;
                        }
                    } else {
                        uint r_id = get_mat_id(src_at(x + 1, y));
                        if (can_displace(self_id, r_id)) tx = x + 1;
                        else {
                            uint l_id = get_mat_id(src_at(x - 1, y));
                            if (can_displace(self_id, l_id)) tx = x - 1;
                        }
                    }
                }
            }
        }

        if (tx != x || ty != y) {
            try_claim(idx, cell_index(tx, ty));
        }
        return;
    }

    if (params.pass_id == 1) {
        // Resolve pass: each target cell pulls from the claimed source.
        uint c = claim.data[idx];
        uvec2 cell;
        if (c > 0u) {
            int src_idx = int(c) - 1;
            cell = src.cells[src_idx];
            uint flags = get_flags(cell);
            flags |= FLAG_MOVED;
            flags &= ~FLAG_SLEEP;
            cell = set_flags(cell, flags);
        } else {
            cell = src.cells[idx];
        }
        dst.cells[idx] = cell;
        return;
    }

    if (params.pass_id == 2) {
        // Clear leftover: if this source moved away and no one claimed its old slot, clear it.
        uint moved = moved_to.data[idx];
        if (moved > 0u && claim.data[idx] == 0u) {
            dst.cells[idx] = uvec2(0u);
        }
        return;
    }

    // Pass 3: reactions, lifetime decay, heat diffusion and phase changes.
    uvec2 cell = dst.cells[idx];
    uint mat_id = get_mat_id(cell);
    if (is_empty(mat_id)) {
        return;
    }

    int temp = get_temperature(cell);
    uint flags = get_flags(cell);
    bool reacted = false;

    if (is_explosive(mat_id) && !reacted) {
        uint explode_to = get_explode_to(mat_id);
        if (explode_to != 0u && has_hot_neighbor(x, y)) {
            cell = set_mat_id(cell, explode_to);
            temp = get_default_temperature(explode_to);
            flags = 0u;
            reacted = true;
        }
    }

    if (is_flammable(mat_id) && !reacted) {
        uint burn_to = get_burn_to(mat_id);
        if (burn_to != 0u && has_hot_neighbor(x, y)) {
            cell = set_mat_id(cell, burn_to);
            temp = get_default_temperature(burn_to);
            flags = 0u;
            reacted = true;
        }
    }

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
    dst.cells[idx] = cell;
}
