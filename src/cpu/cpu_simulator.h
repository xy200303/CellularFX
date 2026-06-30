#ifndef CPU_SIMULATOR_H
#define CPU_SIMULATOR_H

#include "core/isimulator.h"
#include "core/world_grid.h"
#include "core/material_registry.h"
#include "thread_pool.h"

#include <godot_cpp/classes/image.hpp>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace ca {

class CPUSimulator : public ISimulator {
private:
	int width = 0;
	int height = 0;
	WorldGrid grid;
	MaterialRegistry *registry = nullptr;
	godot::Ref<godot::Image> image;
	std::mt19937 rng;
	uint32_t frame_counter = 0;

	bool active_region_valid = false;
	int active_min_x = 0;
	int active_max_x = 0;
	int active_min_y = 0;
	int active_max_y = 0;

	int thread_count = 4;
	mutable std::unique_ptr<ThreadPool> thread_pool;

	struct Region {
		bool valid = false;
		int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
	};

	bool can_move_to(int p_x, int p_y, uint16_t p_current_mat) const;
	void move_particle(int p_x, int p_y, int p_tx, int p_ty);

	uint16_t resolve_material_name(const std::string &p_name) const;
	bool is_hot_neighbor(int p_x, int p_y) const;
	void apply_reactions(int p_x, int p_y);

	void thermal_init_band(int p_min_x, int p_max_x, int p_min_y, int p_max_y);
	void thermal_compute_band(int p_min_x, int p_max_x, int p_min_y, int p_max_y);

	void reset_active_region();
	void expand_active_region(int p_x, int p_y);
	static void include_region(Region &r, int p_x, int p_y);
	static void merge_region(Region &r_dst, const Region &r_src);

	const MaterialRegistry &registry_ref() const {
		static MaterialRegistry s_dummy;
		return registry ? *registry : s_dummy;
	}

public:
	CPUSimulator();
	~CPUSimulator() override;

	bool initialize(int p_width, int p_height, MaterialRegistry &p_registry) override;
	void update() override;
	void set_cell(int p_x, int p_y, uint16_t p_material_id) override;
	uint16_t get_cell(int p_x, int p_y) const override;
	godot::Ref<godot::Image> render_image() override;
	int get_particle_count() const override;
	void clear() override;
	void shutdown();

	godot::PackedByteArray serialize() const override;
	bool deserialize(const godot::PackedByteArray &p_data) override;
};

} // namespace ca

#endif // CPU_SIMULATOR_H
