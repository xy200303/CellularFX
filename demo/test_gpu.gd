extends SceneTree

const MAT_STONE := "stone"
const MAT_FIRE := "fire"
const MAT_SMOKE := "smoke"
const MAT_WOOD := "wood"

func _init():
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_GPU)
	world.init(64, 64)

	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, MAT_FIRE, CASMaterial.TYPE_GAS, Color(1.0, 0.4, 0.1), 0, 60, MAT_SMOKE)
	register_material(world, MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.4, 0.4, 0.4), 0, 20, "")
	register_material(world, MAT_WOOD, CASMaterial.TYPE_SOLID, Color(0.55, 0.27, 0.07), 10, 0, "", true, MAT_FIRE)

	print("Backend: ", world.get_backend_name())
	if world.get_backend_name() == "CPU":
		print("GPU not available in this environment, CPU fallback active.")
		quit(0)

	# Basic GPU test: fire rises.
	world.clear()
	world.set_cell(32, 50, MAT_FIRE)
	for i in range(10):
		world.update()
	var fire_y := find_first_y(world, MAT_FIRE)
	if fire_y >= 0 and fire_y < 50:
		print("GPU basic test passed: fire rises to y=", fire_y)
	else:
		push_error("GPU basic test failed: fire did not rise, y=" + str(fire_y))
		quit(1)

	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int, lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "", corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1, explosive: bool = false, explode_to: String = "") -> void:
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	mat.lifetime = lifetime
	mat.decay_to = decay_to
	mat.flammable = flammable
	mat.burn_to = burn_to
	mat.corrosive = corrosive
	mat.corrosion_residue = corrosion_residue
	mat.corrosion_chance = corrosion_chance
	mat.explosive = explosive
	mat.explode_to = explode_to
	world.register_material(mat)

func find_first_y(world: CASWorld, name: String) -> int:
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) == name:
				return y
	return -1
