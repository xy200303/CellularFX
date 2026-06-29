@tool
extends EditorPlugin

const DockScene := preload("res://addons/cellular_automata_engine/dock.gd")

var dock: Control = null

func _enter_tree() -> void:
	dock = DockScene.new()
	dock.name = "CellularFXDock"
	add_control_to_dock(EditorPlugin.DOCK_SLOT_RIGHT_UL, dock)

	var selection := EditorInterface.get_selection()
	selection.selection_changed.connect(_on_selection_changed)
	_update_dock_world()


func _exit_tree() -> void:
	if dock != null:
		remove_control_from_docks(dock)
		dock.queue_free()
		dock = null


func _on_selection_changed() -> void:
	_update_dock_world()


func _update_dock_world() -> void:
	if dock == null:
		return
	var selected := EditorInterface.get_selection().get_selected_nodes()
	var found: CASWorld = null
	for node in selected:
		if node is CASWorld:
			found = node
			break
	dock.set_world(found)
