extends SceneTree

func _init() -> void:
	var scene := load("res://demo/games/dungeon_crawl/dungeon_crawl.tscn")
	if scene == null:
		push_error("Failed to load dungeon_crawl.tscn")
		quit(1)
		return

	var instance: Node = scene.instantiate()
	root.add_child(instance)

	await process_frame
	await process_frame

	var img := root.get_texture().get_image()
	var err := img.save_png("res://screenshots/dungeon_crawl.png")
	if err == OK:
		print("Dungeon crawl screenshot saved")
	else:
		push_error("Failed to save screenshot")

	quit(0)
