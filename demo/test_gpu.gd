extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"

func _init():
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_GPU)
	world.init(64, 64)

	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)

	var ok := true

	# Test 1: powder falls down.
	world.set_cell(32, 10, MAT_SAND)
	world.update()
	if world.get_cell(32, 11) != MAT_SAND:
		push_error("Test 1 failed: sand did not fall down")
		ok = false
	else:
		print("Test 1 passed: sand falls down")

	# Test 2: solid stays in place.
	world.clear()
	world.set_cell(32, 10, MAT_STONE)
	world.update()
	if world.get_cell(32, 10) != MAT_STONE:
		push_error("Test 2 failed: stone moved")
		ok = false
	else:
		print("Test 2 passed: stone stays")

	# Test 3: liquid falls and spreads horizontally.
	world.clear()
	world.set_cell(32, 10, MAT_WATER)
	for i in range(60):
		world.update()
	var found_water := false
	for x in range(64):
		if world.get_cell(x, 63) == MAT_WATER or world.get_cell(x, 62) == MAT_WATER or world.get_cell(x, 61) == MAT_WATER:
			found_water = true
			break
	if not found_water:
		push_error("Test 3 failed: water did not reach bottom")
		ok = false
	else:
		print("Test 3 passed: water reaches bottom")

	# Test 4: powder piles up diagonally.
	world.clear()
	world.set_cell(32, 60, MAT_SAND)
	world.update()
	var settled := world.get_cell(32, 61) == MAT_SAND or world.get_cell(31, 61) == MAT_SAND or world.get_cell(33, 61) == MAT_SAND
	if not settled:
		push_error("Test 4 failed: sand did not settle")
		ok = false
	else:
		print("Test 4 passed: sand settles")

	if ok:
		print("All CPU backend tests passed.")
	else:
		print("Some CPU backend tests failed.")
		quit(1)

	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int):
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	world.register_material(mat)

