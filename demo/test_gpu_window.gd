extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"

func _init():
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_GPU)
	world.init(64, 64)

	print("Backend: ", world.get_backend_name())

	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)

	# Test vertical fall.
	world.set_cell(32, 10, MAT_SAND)
	world.update()
	var sand_y := find_first_y(world, MAT_SAND)
	if sand_y == 11:
		print("GPU Test 1 passed: sand falls down")
	else:
		push_error("GPU Test 1 failed: sand at y=" + str(sand_y))

	# Test solid stays.
	world.clear()
	world.set_cell(32, 10, MAT_STONE)
	world.update()
	if world.get_cell(32, 10) == MAT_STONE:
		print("GPU Test 2 passed: stone stays")
	else:
		push_error("GPU Test 2 failed: stone moved")

	# Test water reaches bottom.
	world.clear()
	world.set_cell(32, 10, MAT_WATER)
	for i in range(60):
		world.update()
	var water_bottom := false
	for x in range(64):
		if world.get_cell(x, 63) == MAT_WATER or world.get_cell(x, 62) == MAT_WATER or world.get_cell(x, 61) == MAT_WATER:
			water_bottom = true
			break
	if water_bottom:
		print("GPU Test 3 passed: water reaches bottom")
	else:
		push_error("GPU Test 3 failed: water did not reach bottom")

	print("GPU tests finished. Quitting.")
	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int):
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	world.register_material(mat)

func find_first_y(world: CASWorld, name: String) -> int:
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) == name:
				return y
	return -1
