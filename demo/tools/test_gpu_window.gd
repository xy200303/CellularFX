extends SceneTree

func _init() -> void:
	var world := CASWorld.new()
	world.set_backend(CASWorld.BACKEND_GPU)
	world.set_force_gpu(true)
	world.init(128, 128)
	print("backend: ", world.get_backend_name())

	register_material(world, "stone", CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)
	register_material(world, "sand", CASMaterial.TYPE_POWDER, Color(0.9, 0.8, 0.5), 5)

	world.clear()
	world.set_cell(64, 10, "sand")
	for i in range(30):
		world.update()

	print("sand y=", find_y(world, "sand"))
	world.free()
	quit(0)

func register_material(world: CASWorld, name: String, type: int, color: Color, density: int) -> void:
	var mat := CASMaterial.new()
	mat.material_name = name
	mat.material_type = type
	mat.material_color = color
	mat.density = density
	world.register_material(mat)

func find_y(world: CASWorld, name: String) -> int:
	var size := world.get_world_size()
	for y in range(size.y):
		for x in range(size.x):
			if world.get_cell(x, y) == name:
				return y
	return -1
