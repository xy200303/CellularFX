#[compute]
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) restrict readonly buffer SrcGrid {
    uint cells[];
} src;

layout(set = 0, binding = 1, std430) restrict buffer DstGrid {
    uint cells[];
} dst;

layout(set = 0, binding = 2, std430) restrict readonly buffer MaterialBuffer {
    uint count;
    uint data[];
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

uint get_type(uint mat_id) {
    if (mat_id == 0u || mat_id >= materials.count) return TYPE_SOLID;
    return materials.data[mat_id] & 0xFFu;
}

uint get_density(uint mat_id) {
    if (mat_id == 0u || mat_id >= materials.count) return 0u;
    return (materials.data[mat_id] >> 8) & 0xFFu;
}

bool is_empty(uint mat_id) { return mat_id == 0u; }
bool is_liquid(uint mat_id) { return get_type(mat_id) == TYPE_LIQUID; }
bool is_powder(uint mat_id) { return get_type(mat_id) == TYPE_POWDER; }
bool is_movable(uint mat_id) {
    uint t = get_type(mat_id);
    return t == TYPE_POWDER || t == TYPE_LIQUID;
}

int cell_index(int x, int y) {
    return y * params.width + x;
}

uint src_at(int x, int y) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return 0u;
    return src.cells[cell_index(x, y)];
}

void clear_dst(int x, int y) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return;
    dst.cells[cell_index(x, y)] = 0u;
}

void store_dst(int x, int y, uint mat_id) {
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) return;
    dst.cells[cell_index(x, y)] = mat_id;
}

void pull_from(int dst_x, int dst_y, int src_x, int src_y) {
    uint mat = src_at(src_x, src_y);
    store_dst(dst_x, dst_y, mat);
    clear_dst(src_x, src_y);
}

void main() {
    int idx = int(gl_GlobalInvocationID.x);
    int total = params.width * params.height;
    if (idx >= total) return;

    int x = idx % params.width;
    int y = idx / params.width;

    uint self = src_at(x, y);

    // Default: copy source to destination.
    store_dst(x, y, self);

    if (!is_empty(self)) {
        return;
    }

    if (params.pass_id == 0) {
        // Vertical fall: pull from directly above.
        uint above = src_at(x, y - 1);
        if (is_movable(above)) {
            pull_from(x, y, x, y - 1);
        }
    } else if (params.pass_id == 1) {
        // Diagonal slide.
        uint above_left = src_at(x - 1, y - 1);
        uint left = src_at(x - 1, y);
        bool can_left = is_movable(above_left) && !is_empty(left);

        uint above_right = src_at(x + 1, y - 1);
        uint right = src_at(x + 1, y);
        bool can_right = is_movable(above_right) && !is_empty(right);

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
        }
    } else if (params.pass_id == 2) {
        // Horizontal flow for liquids.
        uint left = src_at(x - 1, y);
        uint right = src_at(x + 1, y);

        bool can_left = is_liquid(left);
        bool can_right = is_liquid(right);

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
}
