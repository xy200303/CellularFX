extends Node2D


@onready var world: CASWorld = $CASWorld
@onready var texture_rect: TextureRect = $TextureRect

const MAT_SAND := "sand"
const MAT_WATER := "water"
const MAT_STONE := "stone"

var current_material := MAT_SAND
var brush_size := 4
var stats_label: Label


func _ready() -> void:
	# To use the GPU backend, uncomment the next line. GPU requires a windowed/GPU-capable Godot process.
	# world.set_backend(CASWorld.BACKEND_GPU)
	world.init(256, 256)

	var sand := CASMaterial.new()
	sand.material_name = MAT_SAND
	sand.material_type = CASMaterial.TYPE_POWDER
	sand.material_color = Color(0.76, 0.70, 0.50)
	sand.density = 5
	world.register_material(sand)

	var water := CASMaterial.new()
	water.material_name = MAT_WATER
	water.material_type = CASMaterial.TYPE_LIQUID
	water.material_color = Color(0.25, 0.50, 1.0)
	water.density = 3
	world.register_material(water)

	var stone := CASMaterial.new()
	stone.material_name = MAT_STONE
	stone.material_type = CASMaterial.TYPE_SOLID
	stone.material_color = Color(0.5, 0.5, 0.5)
	stone.density = 10
	world.register_material(stone)

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
	stats_label.text = "Backend: %s\nFPS: %d\nParticles: %d\nBrush: %s (1-3 switch, Space clear)" % [world.get_backend_name(), fps, count, current_material]


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
			KEY_SPACE:
				world.clear()
				print("Cleared")


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
