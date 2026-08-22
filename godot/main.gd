extends Node


const Renderer = preload("res://region_debug_renderer.gd")
const EventPanelScene = preload("res://event_panel.tscn")
const LAYER_NAME_KEYS := [
	"ui.layer.base",
	"ui.layer.relief",
	"ui.layer.elevation",
	"ui.layer.temperature",
	"ui.layer.moisture",
	"ui.layer.feature",
	"ui.layer.edge",
	"ui.layer.settlement",
	"ui.layer.faction",
	"ui.layer.portal",
]

var _core: AetheriaCore
var _seed_edit: LineEdit
var _region_edit: LineEdit
var _map: TextureRect
var _status: Label
var _event_host: VBoxContainer
var _event_panel: PanelContainer
var _layer_buttons: Array[CheckButton] = []
var _layers: Array[bool] = [true, true, false, false, false, true, true, true, true, true]
var _export_path := ""
var _event_dump_requested := false


func _ready() -> void:
	_core = AetheriaCore.new()
	add_child(_core)
	_build_ui()
	_read_command_line()
	call_deferred("_generate")


func _build_ui() -> void:
	var root := VBoxContainer.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 6)
	add_child(root)

	var inputs := HBoxContainer.new()
	root.add_child(inputs)
	inputs.add_child(_label(tr("ui.seed")))
	_seed_edit = LineEdit.new()
	_seed_edit.text = "515151"
	_seed_edit.custom_minimum_size.x = 180
	inputs.add_child(_seed_edit)
	inputs.add_child(_label(tr("ui.region_id")))
	_region_edit = LineEdit.new()
	_region_edit.text = "51"
	_region_edit.custom_minimum_size.x = 100
	inputs.add_child(_region_edit)
	var generate_button := Button.new()
	generate_button.text = tr("ui.regenerate")
	generate_button.pressed.connect(_generate)
	inputs.add_child(generate_button)
	var rebuild_button := Button.new()
	rebuild_button.text = tr("ui.rebuild_events")
	rebuild_button.pressed.connect(_rebuild_event_panel)
	inputs.add_child(rebuild_button)
	inputs.add_child(_label(tr("ui.layer_help")))

	var toggles := HBoxContainer.new()
	root.add_child(toggles)
	for index in LAYER_NAME_KEYS.size():
		var toggle := CheckButton.new()
		toggle.text = "%d %s" % [index + 1, tr(LAYER_NAME_KEYS[index])]
		toggle.button_pressed = _layers[index]
		toggle.toggled.connect(func(enabled: bool) -> void: _set_layer(index, enabled))
		toggles.add_child(toggle)
		_layer_buttons.append(toggle)
	root.add_child(_label(tr("ui.portal_legend")))

	_status = Label.new()
	root.add_child(_status)
	_event_host = VBoxContainer.new()
	root.add_child(_event_host)
	_rebuild_event_panel()
	_map = TextureRect.new()
	_map.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_map.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_map.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_map.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	root.add_child(_map)


func _label(text: String) -> Label:
	var label := Label.new()
	label.text = text
	return label


func _read_command_line() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--seed="):
			_seed_edit.text = argument.trim_prefix("--seed=")
		elif argument.begins_with("--region="):
			_region_edit.text = argument.trim_prefix("--region=")
		elif argument.begins_with("--export-png="):
			_export_path = argument.trim_prefix("--export-png=")
		elif argument.begins_with("--layers="):
			_apply_layer_argument(argument.trim_prefix("--layers="))
		elif argument == "--event-dump":
			_event_dump_requested = true


func _apply_layer_argument(value: String) -> void:
	_layers.fill(false)
	for part in value.split(",", false):
		var layer := part.to_int() - 1
		if layer >= 0 and layer < _layers.size():
			_layers[layer] = true
	for index in _layer_buttons.size():
		_layer_buttons[index].set_pressed_no_signal(_layers[index])


func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo:
		return
	if get_viewport().gui_get_focus_owner() is LineEdit:
		return
	if event.keycode >= KEY_1 and event.keycode <= KEY_9:
		var index: int = event.keycode - KEY_1
		_layer_buttons[index].button_pressed = not _layer_buttons[index].button_pressed
		get_viewport().set_input_as_handled()
	elif event.keycode == KEY_0:
		_layer_buttons[9].button_pressed = not _layer_buttons[9].button_pressed
		get_viewport().set_input_as_handled()
	elif event.keycode == KEY_R:
		_generate()
		get_viewport().set_input_as_handled()


func _set_layer(index: int, enabled: bool) -> void:
	_layers[index] = enabled
	_generate()


func _rebuild_event_panel() -> bool:
	var before := ""
	if is_instance_valid(_event_panel):
		before = _event_panel.dump_text()
		_event_panel.free()
	_event_panel = EventPanelScene.instantiate()
	_event_host.add_child(_event_panel)
	_event_panel.render(_core)
	return before.is_empty() or before == _event_panel.dump_text()


func _generate() -> void:
	if not _seed_edit.text.is_valid_int() or not _region_edit.text.is_valid_int():
		_finish_with_error(tr("ui.error.integer"))
		return
	var seed := _seed_edit.text.to_int()
	var region_id := _region_edit.text.to_int()
	var snapshot: Dictionary = _core.generate_region(seed, region_id)
	if snapshot.has("error"):
		_finish_with_error(snapshot["error"])
		return
	var image := Renderer.render(snapshot, _layers)
	_map.texture = ImageTexture.create_from_image(image)
	var portal_count: int = snapshot["portals"]["x"].size()
	_status.text = tr("ui.region_status").format({
		"seed": seed,
		"region": region_id,
		"width": snapshot["width"],
		"height": snapshot["height"],
		"portals": portal_count,
		"layers": _enabled_layers(),
	})
	if not _export_path.is_empty():
		var absolute_path := ProjectSettings.globalize_path(_export_path)
		DirAccess.make_dir_recursive_absolute(absolute_path.get_base_dir())
		var error := image.save_png(absolute_path)
		if error != OK:
			_finish_with_error(tr("ui.error.png_export").format({
				"error": error,
				"path": absolute_path,
			}))
			return
		print("Region PNG exported: ", absolute_path)
	if _event_dump_requested:
		var rebuild_matches := _rebuild_event_panel()
		print("NARRATIVE_LOCALE=", TranslationServer.get_locale())
		print("NARRATIVE_EVENT_DUMP_BEGIN")
		print(_event_panel.dump_text())
		print("NARRATIVE_EVENT_DUMP_END")
		print("NARRATIVE_REBUILD_MATCH=", int(rebuild_matches))
	if DisplayServer.get_name() == "headless":
		get_tree().quit()


func _enabled_layers() -> String:
	var enabled: PackedStringArray
	for index in _layers.size():
		if _layers[index]:
			enabled.append(str(index + 1))
	return ",".join(enabled)


func _finish_with_error(message: String) -> void:
	_status.text = message
	push_error(message)
	if DisplayServer.get_name() == "headless":
		get_tree().quit(1)
