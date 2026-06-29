@tool
extends Control

signal material_selected(material_name: String)

var world: CASWorld = null
var play_button: Button = null
var step_button: Button = null
var clear_button: Button = null
var preview: TextureRect = null
var material_list: ItemList = null
var material_option: OptionButton = null
var brush_size_slider: HSlider = null
var brush_circle: CheckBox = null
var update_timer: Timer = null
var world_label: Label = null

var _playing: bool = false
var _brush_material: String = ""
var _brush_size: int = 3
var _brush_circle: bool = true

func _ready() -> void:
	custom_minimum_size = Vector2(240, 400)
	anchors_preset = Control.PRESET_FULL_RECT

	var scroll := ScrollContainer.new()
	scroll.anchors_preset = Control.PRESET_FULL_RECT
	add_child(scroll)

	var root := VBoxContainer.new()
	root.anchors_preset = Control.PRESET_FULL_RECT
	scroll.add_child(root)

	# Header
	var title := Label.new()
	title.text = "CellularFX"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 16)
	root.add_child(title)

	# World status
	world_label = Label.new()
	world_label.name = "WorldLabel"
	world_label.text = "No CASWorld selected"
	world_label.autowrap_mode = TextServer.AUTOWRAP_WORD
	root.add_child(world_label)

	# Preview
	preview = TextureRect.new()
	preview.name = "Preview"
	preview.custom_minimum_size = Vector2(200, 200)
	preview.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	preview.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	preview.gui_input.connect(_on_preview_gui_input)
	root.add_child(preview)

	# Playback controls
	var hbox := HBoxContainer.new()
	root.add_child(hbox)

	play_button = Button.new()
	play_button.text = "Play"
	play_button.toggled.connect(_on_play_toggled)
	play_button.toggle_mode = true
	hbox.add_child(play_button)

	step_button = Button.new()
	step_button.text = "Step"
	step_button.pressed.connect(_on_step_pressed)
	hbox.add_child(step_button)

	clear_button = Button.new()
	clear_button.text = "Clear"
	clear_button.pressed.connect(_on_clear_pressed)
	hbox.add_child(clear_button)

	# Brush settings
	var brush_title := Label.new()
	brush_title.text = "Brush"
	brush_title.add_theme_font_size_override("font_size", 14)
	root.add_child(brush_title)

	var material_label := Label.new()
	material_label.text = "Material"
	root.add_child(material_label)

	material_option = OptionButton.new()
	material_option.item_selected.connect(_on_material_selected)
	root.add_child(material_option)

	var size_label := Label.new()
	size_label.text = "Size"
	root.add_child(size_label)

	brush_size_slider = HSlider.new()
	brush_size_slider.min_value = 1
	brush_size_slider.max_value = 20
	brush_size_slider.value = _brush_size
	brush_size_slider.value_changed.connect(_on_brush_size_changed)
	root.add_child(brush_size_slider)

	brush_circle = CheckBox.new()
	brush_circle.text = "Circle"
	brush_circle.button_pressed = _brush_circle
	brush_circle.toggled.connect(_on_brush_circle_toggled)
	root.add_child(brush_circle)

	# Materials panel
	var mat_title := Label.new()
	mat_title.text = "Project Materials"
	mat_title.add_theme_font_size_override("font_size", 14)
	root.add_child(mat_title)

	material_list = ItemList.new()
	material_list.custom_minimum_size = Vector2(0, 120)
	material_list.item_selected.connect(_on_material_list_selected)
	root.add_child(material_list)

	var create_mat_button := Button.new()
	create_mat_button.text = "Create Material"
	create_mat_button.pressed.connect(_on_create_material_pressed)
	root.add_child(create_mat_button)

	_refresh_materials()


func set_world(p_world: CASWorld) -> void:
	world = p_world
	if world_label == null:
		return
	if world == null:
		world_label.text = "No CASWorld selected"
		_preview_texture(null)
	else:
		world_label.text = "World: %s (%dx%d)" % [world.name, world.get_world_size().x, world.get_world_size().y]
		_refresh_registry_materials()
		_update_preview()


func _process(_delta: float) -> void:
	if _playing and world != null:
		world.update()
		_update_preview()


func _update_preview() -> void:
	if world == null:
		return
	var tex := world.get_texture()
	_preview_texture(tex)


func _preview_texture(tex: Texture2D) -> void:
	preview.texture = tex


func _on_play_toggled(pressed: bool) -> void:
	_playing = pressed
	play_button.text = "Pause" if _playing else "Play"


func _on_step_pressed() -> void:
	if world == null:
		return
	world.update()
	_update_preview()


func _on_clear_pressed() -> void:
	if world == null:
		return
	world.clear()
	_update_preview()


func _on_material_selected(index: int) -> void:
	_brush_material = material_option.get_item_text(index)


func _on_brush_size_changed(value: float) -> void:
	_brush_size = int(value)


func _on_brush_circle_toggled(pressed: bool) -> void:
	_brush_circle = pressed


func _on_material_list_selected(index: int) -> void:
	var text := material_list.get_item_text(index)
	material_selected.emit(text)


func _on_create_material_pressed() -> void:
	var dialog := EditorFileDialog.new()
	dialog.file_mode = EditorFileDialog.FILE_MODE_SAVE_FILE
	dialog.add_filter("*.tres", "Godot Resource")
	dialog.file_selected.connect(_on_material_file_selected)
	add_child(dialog)
	dialog.popup_centered(Vector2(600, 400))


func _on_material_file_selected(path: String) -> void:
	var mat := CASMaterial.new()
	mat.material_name = path.get_file().get_basename()
	var err := ResourceSaver.save(mat, path)
	if err != OK:
		push_error("Failed to save material: " + path)
	else:
		_refresh_materials()


func _refresh_materials() -> void:
	material_list.clear()
	var files := _scan_materials("res://", [])
	for f in files:
		material_list.add_item(f)


func _scan_materials(dir_path: String, acc: Array) -> Array:
	var dir := DirAccess.open(dir_path)
	if dir == null:
		return acc
	dir.list_dir_begin()
	var file := dir.get_next()
	while file != "":
		if dir.current_is_dir():
			if not file.begins_with("."):
				_scan_materials(dir_path.path_join(file), acc)
		else:
			if file.ends_with(".tres"):
				var full := dir_path.path_join(file)
				var res := ResourceLoader.load(full, "CASMaterial", ResourceLoader.CACHE_MODE_IGNORE)
				if res is CASMaterial:
					acc.append(full)
		file = dir.get_next()
	return acc


func _refresh_registry_materials() -> void:
	material_option.clear()
	if world == null:
		return
	# Refresh from the world registry by querying known materials indirectly.
	# Since there is no public registry API, we scan the project for CASMaterial
	# resources and assume they have been registered by the user scene.
	var files := _scan_materials("res://", [])
	for f in files:
		var res := ResourceLoader.load(f, "CASMaterial", ResourceLoader.CACHE_MODE_IGNORE) as CASMaterial
		if res != null:
			material_option.add_item(res.material_name)
	if material_option.item_count > 0:
		_brush_material = material_option.get_item_text(0)


func _on_preview_gui_input(event: InputEvent) -> void:
	if world == null:
		return
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			_paint_at(event.position)
	elif event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
			_paint_at(event.position)


func _paint_at(pos: Vector2) -> void:
	if world == null or _brush_material.is_empty():
		return
	var size := world.get_world_size()
	var rect := preview.get_rect()
	var uv := (pos - rect.position) / rect.size
	uv.x = clampf(uv.x, 0.0, 1.0)
	uv.y = clampf(uv.y, 0.0, 1.0)
	var cx := int(uv.x * size.x)
	var cy := int(uv.y * size.y)
	var r := _brush_size
	for dy in range(-r, r + 1):
		for dx in range(-r, r + 1):
			if _brush_circle and (dx * dx + dy * dy) > r * r:
				continue
			world.set_cell(cx + dx, cy + dy, _brush_material)
	_update_preview()
