extends Node2D


@onready var world: CASWorld = $CASWorld
@onready var texture_rect: TextureRect = $TextureRect

const MAT_PLAYER := "player"
const MAT_LAVA := "lava"
const MAT_STONE := "stone"
const MAT_SMOKE := "smoke"

const WORLD_W := 96
const WORLD_H := 128

var player_pos := Vector2i(48, 120)
var move_timer := 0.0
var move_delay := 0.08
var jump_cooldown := 0.0
var game_over := false
var game_won := false

var score := 0
var high_score := 0

var status_label: Label


func _ready() -> void:
	world.init(WORLD_W, WORLD_H)

	register_material(MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.45, 0.45, 0.48), 100)
	register_material(MAT_PLAYER, CASMaterial.TYPE_SOLID, Color(0.2, 0.8, 0.4), 100)
	register_material(MAT_LAVA, CASMaterial.TYPE_LIQUID, Color(1.0, 0.25, 0.05), 8,
		0, "", false, "", false, "", 0.0, false, "", 1200, 50)
	register_material(MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.35, 0.35, 0.35), 0, 80, "")

	texture_rect.texture = world.get_texture()

	build_level()
	set_player(player_pos)

	status_label = Label.new()
	status_label.position = Vector2(10, 10)
	status_label.add_theme_color_override("font_color", Color(1, 1, 1))
	status_label.add_theme_font_size_override("font_size", 18)
	add_child(status_label)


func _process(delta: float) -> void:
	if game_over or game_won:
		return

	score += 1

	move_timer += delta
	jump_cooldown = max(0.0, jump_cooldown - delta)

	var moved := false
	if move_timer >= move_delay:
		move_timer = 0.0
		var new_pos := player_pos
		if Input.is_action_pressed("ui_left"):
			new_pos.x -= 1
		elif Input.is_action_pressed("ui_right"):
			new_pos.x += 1
		if new_pos != player_pos and can_move_to(new_pos):
			move_player(new_pos, false)
			moved = true

	if Input.is_action_just_pressed("ui_up") and jump_cooldown <= 0.0:
		var up := player_pos + Vector2i(0, -1)
		if can_move_to(up):
			move_player(up, true)
			jump_cooldown = 0.18

	spawn_lava()
	world.update()

	if check_player_died():
		game_over = true
		if score > high_score:
			high_score = score
		status_label.text = "GAME OVER\nScore: %d\nHigh: %d\nPress R to restart" % [score, high_score]
		return

	if player_pos.y < 10:
		game_won = true
		score += 1000
		if score > high_score:
			high_score = score
		status_label.text = "YOU ESCAPED!\nScore: %d\nHigh: %d\nPress R to restart" % [score, high_score]
		return

	status_label.text = "Lava Escape\n← → move, ↑ jump\nScore: %d  High: %d" % [score, high_score]


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_R:
		get_tree().reload_current_scene()


func register_material(name: String, type: int, color: Color, density: int,
		lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "",
		corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1,
		explosive: bool = false, explode_to: String = "", temperature: int = 20,
		thermal_conductivity: int = 10) -> void:
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


func build_level() -> void:
	world.clear()
	# Side walls.
	for y in range(WORLD_H):
		world.set_cell(0, y, MAT_STONE)
		world.set_cell(WORLD_W - 1, y, MAT_STONE)

	# Ceiling with exit gap.
	for x in range(WORLD_W):
		if x < 38 or x > 58:
			world.set_cell(x, 0, MAT_STONE)
			world.set_cell(x, 1, MAT_STONE)

	# Some platforms / obstacles.
	for x in range(15, 35):
		world.set_cell(x, 40, MAT_STONE)
	for x in range(60, 82):
		world.set_cell(x, 55, MAT_STONE)
	for x in range(20, 50):
		world.set_cell(x, 80, MAT_STONE)
	for x in range(62, 88):
		world.set_cell(x, 100, MAT_STONE)


func set_player(pos: Vector2i) -> void:
	player_pos = pos
	world.set_cell(pos.x, pos.y, MAT_PLAYER)


func can_move_to(pos: Vector2i) -> bool:
	if pos.x < 1 or pos.x >= WORLD_W - 1 or pos.y < 0 or pos.y >= WORLD_H:
		return false
	var mat := world.get_cell(pos.x, pos.y)
	return mat == "" or mat == MAT_SMOKE


func move_player(to: Vector2i, is_jump: bool) -> void:
	var dy := player_pos.y - to.y
	if dy > 0:
		# Reward climbing upward.
		score += 50 * dy
	if is_jump:
		score += 25

	world.set_cell(player_pos.x, player_pos.y, "")
	player_pos = to
	world.set_cell(player_pos.x, player_pos.y, MAT_PLAYER)


func spawn_lava() -> void:
	# Lava drips from the ceiling, mostly around the center exit.
	if randf() < 0.35:
		var x := randi_range(38, 58)
		if world.get_cell(x, 2) == "":
			world.set_cell(x, 2, MAT_LAVA)
	# Extra drips from the sides.
	if randf() < 0.15:
		var x := randi_range(5, 30)
		if world.get_cell(x, 2) == "":
			world.set_cell(x, 2, MAT_LAVA)
	if randf() < 0.15:
		var x := randi_range(66, WORLD_W - 6)
		if world.get_cell(x, 2) == "":
			world.set_cell(x, 2, MAT_LAVA)


func check_player_died() -> bool:
	for dx in range(-1, 2):
		for dy in range(-1, 2):
			if dx == 0 and dy == 0:
				continue
			var nx := player_pos.x + dx
			var ny := player_pos.y + dy
			if world.get_cell(nx, ny) == MAT_LAVA:
				return true
	return false
