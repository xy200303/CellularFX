extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"
const MAT_FIRE := "fire"
const MAT_SMOKE := "smoke"
const MAT_WOOD := "wood"
const MAT_OIL := "oil"
const MAT_ACID := "acid"

func _init() -> void:
	capture_scene_a()
	capture_scene_b()
	quit(0)


func capture_scene_a() -> void:
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_CPU)
	world.init(128, 128)
	register_materials(world)

	# Small interactive scene: sand pile, water pool, wood tower, oil fire, acid corrosion.
	for x in range(128):
		world.set_cell(x, 120, MAT_STONE)
	for y in range(80, 121):
		world.set_cell(20, y, MAT_STONE)
		world.set_cell(107, y, MAT_STONE)

	for x in range(25, 60):
		for y in range(100, 120):
			if randf() < 0.7:
				world.set_cell(x, y, MAT_SAND)

	for x in range(65, 100):
		for y in range(110, 120):
			world.set_cell(x, y, MAT_WATER)

	for y in range(85, 120):
		world.set_cell(50, y, MAT_WOOD)
		world.set_cell(51, y, MAT_WOOD)

	for x in range(45, 57):
		for y in range(75, 85):
			world.set_cell(x, y, MAT_OIL)
	world.set_cell(51, 74, MAT_FIRE)

	for y in range(10, 40):
		world.set_cell(15, y, MAT_ACID)

	for i in range(120):
		world.update()

	save_world_image(world, "res://screenshots/image.png")
	world.free()


func capture_scene_b() -> void:
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_CPU)
	world.init(256, 256)
	register_materials(world)

	# Larger world: layered terrain with caves, water flow, fire spreading through wood.
	for x in range(256):
		world.set_cell(x, 240, MAT_STONE)
		if x > 40 and x < 216 and randf() < 0.02:
			world.set_cell(x, 239, MAT_STONE)

	# A cave ceiling.
	for x in range(60, 196):
		for y in range(180, 190):
			if randf() < 0.85:
				world.set_cell(x, y, MAT_STONE)

	# Sand falls from top-left.
	for x in range(30, 90):
		for y in range(10, 40):
			if randf() < 0.5:
				world.set_cell(x, y, MAT_SAND)

	# Water reservoir at top-right.
	for x in range(170, 230):
		for y in range(20, 60):
			if randf() < 0.6:
				world.set_cell(x, y, MAT_WATER)

	# Wood forest at bottom.
	for x in range(80, 176):
		if x % 6 < 2:
			for y in range(210, 240):
				world.set_cell(x, y, MAT_WOOD)

	# Ignite the forest.
	world.set_cell(110, 205, MAT_FIRE)
	world.set_cell(140, 205, MAT_FIRE)

	for i in range(200):
		world.update()

	save_world_image(world, "res://screenshots/cellularfx_screenshot.png")
	world.free()


func save_world_image(world: CASWorld, path: String) -> void:
	var img := world.get_image()
	img.resize(512, 512, Image.INTERPOLATE_NEAREST)
	var err := img.save_png(path)
	if err != OK:
		push_error("Failed to save screenshot: " + path)
	else:
		print("Screenshot saved to " + path)


func register_materials(world: CASWorld) -> void:
	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, MAT_FIRE, CASMaterial.TYPE_GAS, Color(1.0, 0.4, 0.1), 0, 30, MAT_SMOKE)
	register_material(world, MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.4, 0.4, 0.4), 0, 90, "")
	register_material(world, MAT_WOOD, CASMaterial.TYPE_SOLID, Color(0.55, 0.27, 0.07), 10, 0, "", true, MAT_FIRE)
	register_material(world, MAT_OIL, CASMaterial.TYPE_LIQUID, Color(0.2, 0.15, 0.1), 2, 0, "", true, MAT_FIRE)
	register_material(world, MAT_ACID, CASMaterial.TYPE_LIQUID, Color(0.5, 1.0, 0.2), 2, 0, "", false, "", true, MAT_SMOKE, 0.15)


func register_material(world: CASWorld, name: String, type: int, color: Color, density: int, lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "", corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1) -> void:
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
	world.register_material(mat)
