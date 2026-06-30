extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"

func _init():
	var world := CASWorld.new()
	world.init(64, 64)

	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)

	world.clear()
	world.set_cell(32, 50, MAT_SAND)
	world.set_cell(33, 50, MAT_WATER)
	world.set_cell(34, 50, MAT_STONE)

	var before := world.get_particle_count()
	var save_path := "user://test_world.cafx"
	var err := world.save_world(save_path)
	if err != OK:
		push_error("save_world failed with error " + str(err))
		world.free()
		quit(1)

	world.clear()
	if world.get_particle_count() != 0:
		push_error("clear did not empty world")
		world.free()
		quit(1)

	err = world.load_world(save_path)
	if err != OK:
		push_error("load_world failed with error " + str(err))
		world.free()
		quit(1)

	var after := world.get_particle_count()
	if after != before:
		push_error("particle count mismatch: before=" + str(before) + " after=" + str(after))
		world.free()
		quit(1)

	if world.get_cell(32, 50) != MAT_SAND:
		push_error("sand not restored at (32,50): " + world.get_cell(32, 50))
		world.free()
		quit(1)
	if world.get_cell(33, 50) != MAT_WATER:
		push_error("water not restored at (33,50): " + world.get_cell(33, 50))
		world.free()
		quit(1)
	if world.get_cell(34, 50) != MAT_STONE:
		push_error("stone not restored at (34,50): " + world.get_cell(34, 50))
		world.free()
		quit(1)

	print("Serialize test passed: saved ", before, " particles and restored ", after)
	world.free()
	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int) -> void:
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	world.register_material(mat)

func count(world: CASWorld) -> int:
	var c := 0
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) != "" and world.get_cell(x, y) != "air":
				c += 1
	return c
