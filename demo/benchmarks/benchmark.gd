extends SceneTree

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"
const MAT_FIRE := "fire"
const MAT_SMOKE := "smoke"

func _init():
	var sizes := [Vector2i(64, 64), Vector2i(128, 128), Vector2i(256, 256), Vector2i(512, 512)]
	var frames := 120

	print("=== CellularFX Benchmark ===")
	print("Backend: CPU (GPU unavailable in headless)")
	print("Frames per size: ", frames)
	print("")

	for size in sizes:
		_run_benchmark(size, frames)

	print("=== Done ===")
	quit(0)

func _run_benchmark(size: Vector2i, frames: int) -> void:
	var world := CASWorld.new()
	world.init(size.x, size.y)

	register_material(world, MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, MAT_FIRE, CASMaterial.TYPE_GAS, Color(1.0, 0.4, 0.1), 0, 60, MAT_SMOKE, false, "", false, "", 0.0, false, "", 300, 100)
	register_material(world, MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.4, 0.4, 0.4), 0, 20, "")

	# Fill bottom half with sand/stone and top with water.
	for y in range(size.y / 2, size.y):
		for x in range(size.x):
			var r := randf()
			if r < 0.7:
				world.set_cell(x, y, MAT_SAND)
			elif r < 0.9:
				world.set_cell(x, y, MAT_STONE)
	for y in range(0, size.y / 4):
		for x in range(size.x):
			if randf() < 0.5:
				world.set_cell(x, y, MAT_WATER)

	var count := world.get_particle_count()

	# Warmup.
	for i in range(10):
		world.update()

	var start := Time.get_ticks_usec()
	for i in range(frames):
		world.update()
	var end := Time.get_ticks_usec()

	var total_ms := (end - start) / 1000.0
	var avg_ms := total_ms / frames
	var fps := 1000.0 / avg_ms

	print("Size %dx%d | particles %d | total %.2f ms | avg %.3f ms/frame | %.1f fps" % [size.x, size.y, count, total_ms, avg_ms, fps])
	world.free()

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
