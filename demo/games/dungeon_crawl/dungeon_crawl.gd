extends Node2D

@onready var world: CASWorld = $CASWorld
@onready var texture_rect: TextureRect = $TextureRect

const MAT_WALL := "wall"
const MAT_PLAYER := "player"
const MAT_ENEMY := "enemy"
const MAT_GOLD := "gold"
const MAT_LAVA := "lava"
const MAT_STAIRS := "stairs"

const WORLD_W := 64
const WORLD_H := 64

const PLAYER_SIZE := Vector2i(2, 2)
const ENEMY_COUNT := 4
const GOLD_COUNT := 6
const LAVA_COUNT := 4

var player_pos := Vector2i(10, 10)
var player_hp := 100
var player_max_hp := 100
var player_attack := 25
var gold := 0
var floor_level := 1

var enemies: Array[Dictionary] = []
var stairs_pos := Vector2i(-1, -1)
var game_over := false

var status_label: Label

func _ready() -> void:
	world.init(WORLD_W, WORLD_H)

	register_material(MAT_WALL, CASMaterial.TYPE_SOLID, Color(0.65, 0.6, 0.5), 100)
	register_material(MAT_PLAYER, CASMaterial.TYPE_SOLID, Color(0.1, 0.95, 0.25), 100,
		0, "", false, "", false, "", 0.0, false, "", 20, 10,
		Color(0.2, 1.0, 0.4), 0, 0, 0, "", "", "", 10, true)
	register_material(MAT_ENEMY, CASMaterial.TYPE_SOLID, Color(0.95, 0.15, 0.15), 100,
		0, "", false, "", false, "", 0.0, false, "", 20, 10,
		Color(1.0, 0.3, 0.3), 0, 0, 0, "", "", "", 0, true)
	register_material(MAT_GOLD, CASMaterial.TYPE_SOLID, Color(1.0, 0.9, 0.1), 100,
		0, "", false, "", false, "", 0.0, false, "", 20, 10,
		Color(1.0, 1.0, 0.4), 0, 0, 0, "", "", "", 0, true)
	register_material(MAT_STAIRS, CASMaterial.TYPE_SOLID, Color(0.2, 0.6, 1.0), 100,
		0, "", false, "", false, "", 0.0, false, "", 20, 10,
		Color(0.4, 0.8, 1.0), 0, 0, 0, "", "", "", 0, true)
	register_material(MAT_LAVA, CASMaterial.TYPE_LIQUID, Color(1.0, 0.35, 0.1), 8,
		0, "", false, "", false, "", 0.0, false, "", 1200, 50,
		Color(1.0, 0.5, 0.2), 0, 0, 0, "", "", "", 0, true)

	texture_rect.texture = world.get_texture()

	status_label = Label.new()
	status_label.position = Vector2(10, 10)
	status_label.add_theme_color_override("font_color", Color(1, 1, 1))
	status_label.add_theme_font_size_override("font_size", 18)
	add_child(status_label)

	start_floor()
	world.update()

func debug_count_materials() -> void:
	var counts := {"": 0, "air": 0, MAT_WALL: 0, MAT_PLAYER: 0, MAT_ENEMY: 0, MAT_GOLD: 0, MAT_STAIRS: 0, MAT_LAVA: 0}
	for y in range(WORLD_H):
		for x in range(WORLD_W):
			var mat := world.get_cell(x, y)
			if counts.has(mat):
				counts[mat] += 1
	print("Material counts: ", counts)

func start_floor() -> void:
	player_hp = player_max_hp
	game_over = false
	enemies.clear()
	generate_dungeon()
	update_ui()

func _input(event: InputEvent) -> void:
	if game_over:
		if event is InputEventKey and event.pressed and event.keycode == KEY_R:
			floor_level = 1
			gold = 0
			start_floor()
		return

	if event is InputEventKey and event.pressed:
		var moved := false
		match event.keycode:
			KEY_LEFT, KEY_A:
				moved = move_player(Vector2i(-1, 0))
			KEY_RIGHT, KEY_D:
				moved = move_player(Vector2i(1, 0))
			KEY_UP, KEY_W:
				moved = move_player(Vector2i(0, -1))
			KEY_DOWN, KEY_S:
				moved = move_player(Vector2i(0, 1))
			KEY_SPACE, KEY_PERIOD:
				moved = true  # wait a turn
			KEY_N:
				if event.ctrl_pressed:
					next_floor()
					return

		if moved:
			apply_hazards()
			move_enemies()
			apply_hazards()
			world.update()
			update_ui()
			check_game_over()

func register_material(name: String, type: int, color: Color, density: int,
		lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "",
		corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1,
		explosive: bool = false, explode_to: String = "", temperature: int = 20,
		thermal_conductivity: int = 10, glow_color: Color = Color(0, 0, 0, 0),
		velocity_x: int = 0, velocity_y: int = 0, melting_point: int = 0,
		solid_form: String = "", liquid_form: String = "", gas_form: String = "",
		freeze_point: int = 0, emit_light: bool = false) -> void:
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
	mat.glow_color = glow_color
	mat.velocity_x = velocity_x
	mat.velocity_y = velocity_y
	mat.melting_point = melting_point
	mat.solid_form = solid_form
	mat.liquid_form = liquid_form
	mat.gas_form = gas_form
	mat.freeze_point = freeze_point
	mat.emit_light = emit_light
	world.register_material(mat)

func generate_dungeon() -> void:
	world.clear()

	# Fill with walls.
	for y in range(WORLD_H):
		for x in range(WORLD_W):
			world.set_cell(x, y, MAT_WALL)

	var rooms: Array[Dictionary] = []
	var floor_tiles: Dictionary = {}
	var attempts := 0
	while rooms.size() < 6 and attempts < 200:
		attempts += 1
		var w := randi_range(6, 12)
		var h := randi_range(6, 10)
		var x := randi_range(2, WORLD_W - w - 3)
		var y := randi_range(2, WORLD_H - h - 3)
		var new_room := Rect2i(x, y, w, h)

		var overlaps := false
		for r in rooms:
			if r["rect"].intersects(new_room.grow(2)):
				overlaps = true
				break
		if overlaps:
			continue

		# Carve room.
		for ry in range(y, y + h):
			for rx in range(x, x + w):
				world.set_cell(rx, ry, "")
				floor_tiles[Vector2i(rx, ry)] = true
		rooms.append({"rect": new_room, "center": Vector2i(x + w / 2, y + h / 2)})

	# Connect rooms with L-shaped corridors.
	for i in range(rooms.size() - 1):
		var a: Vector2i = rooms[i].center
		var b: Vector2i = rooms[i + 1].center
		if randf() < 0.5:
			carve_h_corridor(a.x, b.x, a.y, floor_tiles)
			carve_v_corridor(a.y, b.y, b.x, floor_tiles)
		else:
			carve_v_corridor(a.y, b.y, a.x, floor_tiles)
			carve_h_corridor(a.x, b.x, b.y, floor_tiles)

	spawn_entities(floor_tiles)

func carve_h_corridor(x1: int, x2: int, y: int, floor_tiles: Dictionary) -> void:
	for x in range(min(x1, x2), max(x1, x2) + 1):
		world.set_cell(x, y, "")
		world.set_cell(x, y + 1, "")
		floor_tiles[Vector2i(x, y)] = true
		floor_tiles[Vector2i(x, y + 1)] = true

func carve_v_corridor(y1: int, y2: int, x: int, floor_tiles: Dictionary) -> void:
	for y in range(min(y1, y2), max(y1, y2) + 1):
		world.set_cell(x, y, "")
		world.set_cell(x + 1, y, "")
		floor_tiles[Vector2i(x, y)] = true
		floor_tiles[Vector2i(x + 1, y)] = true

func spawn_entities(floor_tiles: Dictionary) -> void:
	var open_tiles := floor_tiles.keys()
	open_tiles.shuffle()

	# Player placement: prefer the center of the region for a 2x2 open spot.
	var center := region_center(floor_tiles)
	player_pos = center
	var best_dist := 999999
	for p in open_tiles:
		if can_place_player(p):
			var d: int = (p - center).length_squared()
			if d < best_dist:
				best_dist = d
				player_pos = p
	set_player(player_pos)

	# Enemies.
	var placed := 0
	for p in open_tiles:
		if placed >= ENEMY_COUNT:
			break
		if distance_to_player(p) < 15:
			continue
		if is_clear(p, Vector2i.ONE):
			world.set_cell(p.x, p.y, MAT_ENEMY)
			enemies.append({"pos": p, "hp": 40 + floor_level * 10, "attack": 10 + floor_level * 2})
			placed += 1

	# Gold.
	placed = 0
	for p in open_tiles:
		if placed >= GOLD_COUNT:
			break
		if world.get_cell(p.x, p.y) != "air":
			continue
		world.set_cell(p.x, p.y, MAT_GOLD)
		placed += 1

	# Lava pools.
	placed = 0
	for p in open_tiles:
		if placed >= LAVA_COUNT:
			break
		if distance_to_player(p) < 12:
			continue
		if world.get_cell(p.x, p.y) == "air":
			world.set_cell(p.x, p.y, MAT_LAVA)
			placed += 1

	# Stairs: far from player.
	var best_stairs := Vector2i(-1, -1)
	var best_stairs_dist := 0
	for p in open_tiles:
		if world.get_cell(p.x, p.y) != "air":
			continue
		var d := distance_to_player(p)
		if d > best_stairs_dist:
			best_stairs_dist = d
			best_stairs = p
	if best_stairs.x >= 0:
		stairs_pos = best_stairs
		world.set_cell(stairs_pos.x, stairs_pos.y, MAT_STAIRS)

func can_place_player(pos: Vector2i) -> bool:
	return is_clear(pos, PLAYER_SIZE)

func is_clear(pos: Vector2i, size: Vector2i) -> bool:
	if pos.x < 1 or pos.x + size.x >= WORLD_W - 1 or pos.y < 1 or pos.y + size.y >= WORLD_H - 1:
		return false
	for dy in range(size.y):
		for dx in range(size.x):
			if world.get_cell(pos.x + dx, pos.y + dy) != "air":
				return false
	return true

func distance_to_player(pos: Vector2i) -> int:
	return abs(pos.x - player_pos.x) + abs(pos.y - player_pos.y)

func region_center(region: Dictionary) -> Vector2i:
	var sum := Vector2i.ZERO
	for p in region:
		sum += p
	return Vector2i(sum.x / region.size(), sum.y / region.size())

func set_player(pos: Vector2i) -> void:
	player_pos = pos
	for dy in range(PLAYER_SIZE.y):
		for dx in range(PLAYER_SIZE.x):
			world.set_cell(pos.x + dx, pos.y + dy, MAT_PLAYER)

func clear_player() -> void:
	for dy in range(PLAYER_SIZE.y):
		for dx in range(PLAYER_SIZE.x):
			world.set_cell(player_pos.x + dx, player_pos.y + dy, "")

func move_player(dir: Vector2i) -> bool:
	var new_pos := player_pos + dir
	var old_pos := player_pos

	# Temporarily clear the old footprint so the destination check does not
	# count the player itself as an obstacle.
	clear_player()

	if not is_clear(new_pos, PLAYER_SIZE):
		# Attack an enemy if blocked by one, then restore position if not moving.
		var enemy_idx := find_enemy_at(new_pos)
		if enemy_idx >= 0:
			damage_enemy(enemy_idx, player_attack)
			set_player(old_pos)
			return true
		set_player(old_pos)
		return false

	# Collect gold under the new footprint.
	for dy in range(PLAYER_SIZE.y):
		for dx in range(PLAYER_SIZE.x):
			var p := Vector2i(new_pos.x + dx, new_pos.y + dy)
			if world.get_cell(p.x, p.y) == MAT_GOLD:
				gold += 1
				world.set_cell(p.x, p.y, "")
			if world.get_cell(p.x, p.y) == MAT_STAIRS:
				next_floor()
				return true
	set_player(new_pos)
	return true

func find_enemy_at(pos: Vector2i) -> int:
	for i in range(enemies.size()):
		var e := enemies[i]
		if e.pos == pos or (pos.x >= e.pos.x and pos.x < e.pos.x + 1 and pos.y >= e.pos.y and pos.y < e.pos.y + 1):
			return i
	return -1

func damage_enemy(idx: int, damage: int) -> void:
	if idx < 0 or idx >= enemies.size():
		return
	enemies[idx].hp -= damage
	if enemies[idx].hp <= 0:
		world.set_cell(enemies[idx].pos.x, enemies[idx].pos.y, "")
		enemies.remove_at(idx)

func move_enemies() -> void:
	for i in range(enemies.size() - 1, -1, -1):
		var e := enemies[i]
		var old_pos: Vector2i = e.pos
		var new_pos := step_toward_player(old_pos)

		if new_pos == old_pos:
			continue

		# Attack if adjacent/overlapping player.
		if is_adjacent_to_player(new_pos):
			player_hp -= e.attack
			continue

		if world.get_cell(new_pos.x, new_pos.y) == "air" or world.get_cell(new_pos.x, new_pos.y) == MAT_GOLD or world.get_cell(new_pos.x, new_pos.y) == MAT_STAIRS:
			world.set_cell(old_pos.x, old_pos.y, "")
			enemies[i].pos = new_pos
			world.set_cell(new_pos.x, new_pos.y, MAT_ENEMY)

func step_toward_player(pos: Vector2i) -> Vector2i:
	var dx := signi(player_pos.x - pos.x)
	var dy := signi(player_pos.y - pos.y)
	var candidates := []
	if dx != 0:
		candidates.append(pos + Vector2i(dx, 0))
	if dy != 0:
		candidates.append(pos + Vector2i(0, dy))
	if candidates.is_empty():
		return pos
	candidates.shuffle()
	for c in candidates:
		if can_enemy_move(c):
			return c
	return pos

func can_enemy_move(pos: Vector2i) -> bool:
	if pos.x < 1 or pos.x >= WORLD_W - 1 or pos.y < 1 or pos.y >= WORLD_H - 1:
		return false
	var mat := world.get_cell(pos.x, pos.y)
	return mat == "air" or mat == MAT_GOLD or mat == MAT_STAIRS

func is_adjacent_to_player(pos: Vector2i) -> bool:
	for dy in range(PLAYER_SIZE.y):
		for dx in range(PLAYER_SIZE.x):
			var pp := Vector2i(player_pos.x + dx, player_pos.y + dy)
			if abs(pos.x - pp.x) <= 1 and abs(pos.y - pp.y) <= 1:
				return true
	return false

func apply_hazards() -> void:
	for dy in range(PLAYER_SIZE.y):
		for dx in range(PLAYER_SIZE.x):
			if world.get_cell(player_pos.x + dx, player_pos.y + dy) == MAT_LAVA:
				player_hp -= 10
				return

func next_floor() -> void:
	floor_level += 1
	player_max_hp += 10
	start_floor()

func check_game_over() -> void:
	if player_hp <= 0:
		player_hp = 0
		game_over = true
		status_label.text = "GAME OVER\nFloor: %d  Gold: %d\nPress R to restart" % [floor_level, gold]

func update_ui() -> void:
	if game_over:
		return
	status_label.text = "Dungeon Crawl  Floor %d\nHP: %d/%d  Gold: %d\nArrows/WASD move, Space wait\nReach stairs (blue)" % [floor_level, player_hp, player_max_hp, gold]

func _process(_delta: float) -> void:
	pass
