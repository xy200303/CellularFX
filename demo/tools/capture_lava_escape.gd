extends SceneTree

func _init() -> void:
	var scene := load("res://demo/games/lava_escape/game_lava_escape.tscn")
	if scene == null:
		push_error("Failed to load game_lava_escape.tscn")
		quit(1)
		return

	var instance: Node = scene.instantiate()
	root.add_child(instance)

	# Wait for _ready() to initialize the world.
	await process_frame

	var world := instance.get_node("CASWorld") as CASWorld
	if world == null:
		push_error("CASWorld not found")
		quit(1)
		return

	# Move the player up periodically and spawn extra lava for a dramatic screenshot.
	for i in range(400):
		if i % 30 == 0:
			var target: Vector2i = instance.player_pos + Vector2i(0, -3)
			instance.move_player(target)
		if i % 5 == 0:
			instance.spawn_lava()
		# Force extra lava at the top for the screenshot.
		if i % 3 == 0:
			for x in range(35, 61):
				if randf() < 0.3:
					world.set_cell(x, 2, "lava")
		await process_frame

	print("player pos:", instance.player_pos)
	print("cell at (50,10):", world.get_cell(50, 10))
	print("cell at (50,50):", world.get_cell(50, 50))
	print("particle count:", world.get_particle_count())

	var img := world.get_image()
	img.resize(512, 512, Image.INTERPOLATE_NEAREST)
	var err := img.save_png("res://screenshots/lava_escape.png")
	if err == OK:
		print("Lava escape screenshot saved")
	else:
		push_error("Failed to save screenshot")

	quit(0)
