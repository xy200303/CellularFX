extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"
const MAT_FIRE := "fire"
const MAT_SMOKE := "smoke"
const MAT_WOOD := "wood"
const MAT_OIL := "oil"
const MAT_ACID := "acid"
const MAT_GUNPOWDER := "gunpowder"
const MAT_ICE := "ice"
const MAT_STEAM := "steam"

func _init():
	var world := CASWorld.new()
	world.init(64, 64)

	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, MAT_FIRE, CASMaterial.TYPE_GAS, Color(1.0, 0.4, 0.1), 0, 60, MAT_SMOKE, false, "", false, "", 0.0, false, "", 300, 100)
	register_material(world, MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.4, 0.4, 0.4), 0, 20, "")
	register_material(world, MAT_WOOD, CASMaterial.TYPE_SOLID, Color(0.55, 0.27, 0.07), 10, 0, "", true, MAT_FIRE)
	register_material(world, MAT_OIL, CASMaterial.TYPE_LIQUID, Color(0.2, 0.15, 0.1), 2, 0, "", true, MAT_FIRE)
	register_material(world, MAT_ACID, CASMaterial.TYPE_LIQUID, Color(0.5, 1.0, 0.2), 2, 0, "", false, "", true, MAT_SMOKE, 0.5)
	register_material(world, MAT_GUNPOWDER, CASMaterial.TYPE_POWDER, Color(0.2, 0.2, 0.2), 3, 0, "", false, "", false, "", 0.0, true, MAT_FIRE)

	# Phase-change materials (registered by directly configuring CASMaterial).
	var water_mat := CASMaterial.new()
	water_mat.material_name = MAT_WATER
	water_mat.material_type = CASMaterial.TYPE_LIQUID
	water_mat.material_color = Color(0.25, 0.50, 1.0)
	water_mat.density = 3
	water_mat.temperature = 20
	water_mat.freeze_point = 0
	water_mat.boiling_point = 100
	water_mat.solid_form = MAT_ICE
	water_mat.gas_form = MAT_STEAM
	water_mat.thermal_conductivity = 40
	world.register_material(water_mat)

	var ice_mat := CASMaterial.new()
	ice_mat.material_name = MAT_ICE
	ice_mat.material_type = CASMaterial.TYPE_SOLID
	ice_mat.material_color = Color(0.7, 0.9, 1.0)
	ice_mat.density = 10
	ice_mat.temperature = -20
	ice_mat.melting_point = 0
	ice_mat.liquid_form = MAT_WATER
	ice_mat.thermal_conductivity = 40
	world.register_material(ice_mat)

	var steam_mat := CASMaterial.new()
	steam_mat.material_name = MAT_STEAM
	steam_mat.material_type = CASMaterial.TYPE_GAS
	steam_mat.material_color = Color(0.8, 0.9, 1.0)
	steam_mat.density = 0
	steam_mat.temperature = 120
	steam_mat.lifetime = 90
	steam_mat.decay_to = MAT_WATER
	steam_mat.thermal_conductivity = 20
	world.register_material(steam_mat)

	var ok := true

	# Test 1: fire rises and decays to smoke.
	world.clear()
	world.set_cell(32, 50, MAT_FIRE)
	for i in range(10):
		world.update()
	var fire_y := find_first_y(world, MAT_FIRE)
	if fire_y >= 0 and fire_y < 50:
		print("Test 1 passed: fire rises")
	else:
		push_error("Test 1 failed: fire did not rise, y=" + str(fire_y))
		ok = false

	# Test 2: wood catches fire when adjacent to fire.
	world.clear()
	world.set_cell(32, 50, MAT_WOOD)
	world.set_cell(33, 50, MAT_FIRE)
	for i in range(5):
		world.update()
	if count_material(world, MAT_FIRE) > 1:
		print("Test 2 passed: wood catches fire")
	else:
		push_error("Test 2 failed: wood did not catch fire")
		ok = false

	# Test 3: acid corrodes stone. Place acid inside the stone block so it cannot flow away.
	world.clear()
	for y in range(45, 50):
		for x in range(30, 35):
			world.set_cell(x, y, MAT_STONE)
	world.set_cell(32, 47, MAT_ACID)
	for i in range(30):
		world.update()
	var stone_count := count_material(world, MAT_STONE)
	if stone_count < 24:
		print("Test 3 passed: acid corrodes stone (remaining: " + str(stone_count) + ")")
	else:
		push_error("Test 3 failed: acid did not corrode stone (remaining: " + str(stone_count) + ")")
		ok = false

	# Test 4: oil is liquid and falls.
	world.clear()
	world.set_cell(32, 10, MAT_OIL)
	world.update()
	if world.get_cell(32, 11) == MAT_OIL:
		print("Test 4 passed: oil falls")
	else:
		push_error("Test 4 failed: oil did not fall")
		ok = false

	# Test 5: gunpowder explodes when adjacent to fire. A small chamber keeps both cells still.
	world.clear()
	for x in range(31, 34):
		world.set_cell(x, 49, MAT_STONE)
		world.set_cell(x, 51, MAT_STONE)
	world.set_cell(31, 50, MAT_FIRE)
	world.set_cell(32, 50, MAT_GUNPOWDER)
	world.set_cell(33, 50, MAT_STONE)
	for i in range(5):
		world.update()
	var fire_count := count_material(world, MAT_FIRE)
	if fire_count > 2:
		print("Test 5 passed: gunpowder explodes (fire count: " + str(fire_count) + ")")
	else:
		push_error("Test 5 failed: gunpowder did not explode (fire count: " + str(fire_count) + ")")
		ok = false

	# Test 6: water heats up and turns into steam when adjacent to fire.
	world.clear()
	for x in range(31, 34):
		world.set_cell(x, 49, MAT_STONE)
		world.set_cell(x, 51, MAT_STONE)
	world.set_cell(30, 50, MAT_STONE)
	world.set_cell(31, 50, MAT_FIRE)
	world.set_cell(32, 50, MAT_WATER)
	world.set_cell(33, 50, MAT_STONE)
	for i in range(60):
		world.update()
	if count_material(world, MAT_STEAM) > 0:
		print("Test 6 passed: water boils into steam")
	else:
		push_error("Test 6 failed: water did not boil into steam")
		ok = false

	# Test 7: ice melts into water when adjacent to fire.
	world.clear()
	for x in range(31, 34):
		world.set_cell(x, 49, MAT_STONE)
		world.set_cell(x, 51, MAT_STONE)
	world.set_cell(30, 50, MAT_STONE)
	world.set_cell(31, 50, MAT_FIRE)
	world.set_cell(32, 50, MAT_ICE)
	world.set_cell(33, 50, MAT_STONE)
	for i in range(60):
		world.update()
	if count_material(world, MAT_WATER) > 0:
		print("Test 7 passed: ice melts into water")
	else:
		push_error("Test 7 failed: ice did not melt")
		ok = false

	if ok:
		print("All material tests passed.")
	else:
		print("Some material tests failed.")
		world.free()
		quit(1)

	world.free()
	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int, lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "", corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1, explosive: bool = false, explode_to: String = "", temperature: int = 20, thermal_conductivity: int = 10) -> void:
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
	mat.temperature = temperature
	mat.thermal_conductivity = thermal_conductivity
	world.register_material(mat)

func find_first_y(world: CASWorld, name: String) -> int:
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) == name:
				return y
	return -1

func count_material(world: CASWorld, name: String) -> int:
	var count := 0
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) == name:
				count += 1
	return count

