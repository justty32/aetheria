extends Node


const Renderer = preload("res://region_debug_renderer.gd")
const LAYER_NAMES := ["基底", "起伏", "高度", "溫度", "濕度", "地物", "邊", "聚落", "勢力", "出境點"]

var _core: AetheriaCore
var _seed_edit: LineEdit
var _region_edit: LineEdit
var _map: TextureRect
var _status: Label
var _layer_buttons: Array[CheckButton] = []
var _layers: Array[bool] = [true, true, false, false, false, true, true, true, true, true]
var _export_path := ""


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
	inputs.add_child(_label("Seed"))
	_seed_edit = LineEdit.new()
	_seed_edit.text = "515151"
	_seed_edit.custom_minimum_size.x = 180
	inputs.add_child(_seed_edit)
	inputs.add_child(_label("Region ID"))
	_region_edit = LineEdit.new()
	_region_edit.text = "51"
	_region_edit.custom_minimum_size.x = 100
	inputs.add_child(_region_edit)
	var generate_button := Button.new()
	generate_button.text = "重新生成（R）"
	generate_button.pressed.connect(_generate)
	inputs.add_child(generate_button)
	inputs.add_child(_label("1～9、0 切換圖層；勾選框即目前輸出內容"))

	var toggles := HBoxContainer.new()
	root.add_child(toggles)
	for index in LAYER_NAMES.size():
		var toggle := CheckButton.new()
		toggle.text = "%d %s" % [index + 1, LAYER_NAMES[index]]
		toggle.button_pressed = _layers[index]
		toggle.toggled.connect(func(enabled: bool) -> void: _set_layer(index, enabled))
		toggles.add_child(toggle)
		_layer_buttons.append(toggle)
	root.add_child(_label("出境點：青框=海路、黃三角=山口、紫 X=地下、白十字=傳送；勢力：紅／藍／黃，黑線=國界"))

	_status = Label.new()
	root.add_child(_status)
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


func _generate() -> void:
	if not _seed_edit.text.is_valid_int() or not _region_edit.text.is_valid_int():
		_finish_with_error("seed 與 region_id 必須是整數")
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
	_status.text = "seed=%d  region=%d  %dx%d 格  portal=%d  圖層=%s" % [seed, region_id, snapshot["width"], snapshot["height"], portal_count, _enabled_layers()]
	if not _export_path.is_empty():
		var absolute_path := ProjectSettings.globalize_path(_export_path)
		DirAccess.make_dir_recursive_absolute(absolute_path.get_base_dir())
		var error := image.save_png(absolute_path)
		if error != OK:
			_finish_with_error("PNG 匯出失敗（error %d）：%s" % [error, absolute_path])
			return
		print("Region PNG exported: ", absolute_path)
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
