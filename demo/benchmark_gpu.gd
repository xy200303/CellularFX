extends SceneTree

const SIZES := [64, 128, 256]
const FRAMES := 120

func _init():
	print("=== CellularFX GPU vs CPU Benchmark ===")
	print("Frames per size: ", FRAMES)
	print("")

	for size in SIZES:
		bench_backend(size, "CPU", CASWorld.BACKEND_CPU)
		bench_backend(size, "GPU", CASWorld.BACKEND_GPU)
		print("")

	print("=== Done ===")
	quit(0)

func bench_backend(size: int, label: String, backend: int):
	var world := CASWorld.new()
	world.set_backend(backend)
	world.init(size, size)

	register_material(world, "sand", CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(world, "water", CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(world, "stone", CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, "fire", CASMaterial.TYPE_GAS, Color(1.0, 0.3, 0.0), 1)
	register_material(world, "smoke", CASMaterial.TYPE_GAS, Color(0.5, 0.5, 0.5), 1)

	var rng := RandomNumberGenerator.new()
	rng.seed = 12345
	for y in range(size):
		for x in range(size):
			var r := rng.randf()
			if r < 0.20:
				world.set_cell(x, y, "stone")
			elif r < 0.55:
				world.set_cell(x, y, "sand")
			elif r < 0.80:
				world.set_cell(x, y, "water")
			elif r < 0.90:
				world.set_cell(x, y, "fire")

	var particles := 0
	for y in range(size):
		for x in range(size):
			if world.get_cell(x, y) != "":
				particles += 1

	var start := Time.get_ticks_usec()
	for i in range(FRAMES):
		world.update()
	var end := Time.get_ticks_usec()

	var total_ms := (end - start) / 1000.0
	var avg_ms := total_ms / FRAMES
	var fps := 1000.0 / avg_ms

	print("Size %dx%d | backend %-3s | particles %d | total %.2f ms | avg %.3f ms/frame | %.1f fps" % [size, size, label, particles, total_ms, avg_ms, fps])

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int):
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	world.register_material(mat)
