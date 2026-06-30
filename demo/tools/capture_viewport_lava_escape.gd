extends SceneTree

func _init() -> void:
	var scene := load("res://demo/games/lava_escape/game_lava_escape.tscn")
	if scene == null:
		push_error("Failed to load game_lava_escape.tscn")
		quit(1)
		return

	var instance: Node = scene.instantiate()
	root.add_child(instance)

	await process_frame
	await process_frame

	var world := instance.get_node("CASWorld") as CASWorld
	if world == null:
		push_error("CASWorld not found")
		quit(1)
		return

	for i in range(120):
		if i % 30 == 0:
			var target: Vector2i = instance.player_pos + Vector2i(0, -3)
			instance.move_player(target, true)
		if i % 5 == 0:
			instance.spawn_lava()
		if i % 3 == 0:
			for x in range(35, 61):
				if randf() < 0.3:
					world.set_cell(x, 2, "lava")
		await process_frame

	var img := root.get_texture().get_image()
	img.resize(512, 512, Image.INTERPOLATE_NEAREST)
	var err := img.save_png("res://screenshots/lava_escape_viewport.png")
	if err == OK:
		print("Viewport screenshot saved")
	else:
		push_error("Failed to save viewport screenshot")

	quit(0)
