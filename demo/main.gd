extends Node2D


@onready var world: CASWorld = $CASWorld
@onready var texture_rect: TextureRect = $TextureRect

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"
const MAT_FIRE := "fire"
const MAT_SMOKE := "smoke"
const MAT_WOOD := "wood"
const MAT_OIL := "oil"
const MAT_ACID := "acid"

var current_material := MAT_SAND
var brush_size := 4
var stats_label: Label


func _ready() -> void:
	# To use the GPU backend, uncomment the next line. GPU requires a windowed/GPU-capable Godot process.
	# world.set_backend(CASWorld.BACKEND_GPU)
	world.init(256, 256)

	register_material(MAT_SAND, CASMaterial.TYPE_POWDER, Color(0.76, 0.70, 0.50), 5)
	register_material(MAT_WATER, CASMaterial.TYPE_LIQUID, Color(0.25, 0.50, 1.0), 3)
	register_material(MAT_STONE, CASMaterial.TYPE_SOLID, Color(0.5, 0.5, 0.5), 10)

	register_material(MAT_FIRE, CASMaterial.TYPE_GAS, Color(1.0, 0.4, 0.1), 0, 30, MAT_SMOKE)
	register_material(MAT_SMOKE, CASMaterial.TYPE_GAS, Color(0.4, 0.4, 0.4), 0, 90, "")
	register_material(MAT_WOOD, CASMaterial.TYPE_SOLID, Color(0.55, 0.27, 0.07), 10, 0, "", true, MAT_FIRE)
	register_material(MAT_OIL, CASMaterial.TYPE_LIQUID, Color(0.2, 0.15, 0.1), 2, 0, "", true, MAT_FIRE)
	register_material(MAT_ACID, CASMaterial.TYPE_LIQUID, Color(0.5, 1.0, 0.2), 2, 0, "", false, "", true, MAT_SMOKE, 0.15)

	texture_rect.texture = world.get_texture()

	stats_label = Label.new()
	stats_label.position = Vector2(10, 10)
	stats_label.add_theme_color_override("font_color", Color(1, 1, 1))
	stats_label.add_theme_font_size_override("font_size", 16)
	add_child(stats_label)


func _process(_delta: float) -> void:
	world.update()
	var fps := Engine.get_frames_per_second()
	var count := world.get_particle_count()
	stats_label.text = "Backend: %s\nFPS: %d\nParticles: %d\nBrush: %s (1-8 switch, Space clear)" % [world.get_backend_name(), fps, count, current_material]


func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		_draw_at_mouse()
	elif event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		_draw_at_mouse()

	if event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_1:
				current_material = MAT_SAND
				print("Brush: sand")
			KEY_2:
				current_material = MAT_WATER
				print("Brush: water")
			KEY_3:
				current_material = MAT_STONE
				print("Brush: stone")
			KEY_4:
				current_material = MAT_FIRE
				print("Brush: fire")
			KEY_5:
				current_material = MAT_SMOKE
				print("Brush: smoke")
			KEY_6:
				current_material = MAT_WOOD
				print("Brush: wood")
			KEY_7:
				current_material = MAT_OIL
				print("Brush: oil")
			KEY_8:
				current_material = MAT_ACID
				print("Brush: acid")
			KEY_SPACE:
				world.clear()
				print("Cleared")


func register_material(name: String, type: int, color: Color, density: int, lifetime: int = 0, decay_to: String = "", flammable: bool = false, burn_to: String = "", corrosive: bool = false, corrosion_residue: String = "", corrosion_chance: float = 0.1) -> void:
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
	world.register_material(mat)


func _draw_at_mouse() -> void:
	var local_pos := texture_rect.get_local_mouse_position()
	var world_size := world.get_world_size()
	var scale_x := float(world_size.x) / texture_rect.size.x
	var scale_y := float(world_size.y) / texture_rect.size.y
	var px := int(local_pos.x * scale_x)
	var py := int(local_pos.y * scale_y)

	for dx in range(-brush_size, brush_size + 1):
		for dy in range(-brush_size, brush_size + 1):
			if dx * dx + dy * dy <= brush_size * brush_size:
				world.set_cell(px + dx, py + dy, current_material)
