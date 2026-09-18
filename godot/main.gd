extends Node


const Probes = preload("res://main_probes.gd")
const UI = preload("res://main_ui.gd")
const Storage = preload("res://main_storage.gd")
const Grid = preload("res://main_grid.gd")
const Format = preload("res://main_format.gd")
const Renderer = preload("res://region_debug_renderer.gd")
const LAYERS: Array[bool] = [true, true, false, false, false, true, true, true, true, true]
const RESIDENCE_NAMES := ["Region 大地圖", "Site 城區", "Local 探索", "Local 地城"]

var _core: AetheriaCore
var _view: Control
var _map: TextureRect
var _seed_edit: LineEdit
var _region_edit: LineEdit
var _slot_edit: LineEdit
var _character_edit: LineEdit
var _title: Label
var _status: Label
var _message: Label
var _coverage_state: Label
var _tile_state: Label
var _battle_report: Label
var _event_log: Label
var _advance_button: Button
var _manual_button: Button
var _auto_button: Button
var _selected_unit_id := 0
var _last_message := "三層入口在左側；每一層都可親自去或交給系統。"
var _mode := ""
var _screenshot_path := ""
var _expect_invalid_accepted := false
var _slot_name := "quicksave"
var _character_name := "A"


func _ready() -> void:
	_core = AetheriaCore.new()
	add_child(_core)
	_read_command_line()
	_build_view()
	_new_game()
	if not _mode.is_empty():
		call_deferred("_run_probe")


func _read_command_line() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--playable-demo="):
			_mode = argument.trim_prefix("--playable-demo=")
		elif argument == "--bridge-negative-probe":
			_mode = "negative"
		elif argument == "--performance-probe":
			_mode = "performance"
		elif argument == "--expect-invalid-accepted":
			_expect_invalid_accepted = true
		elif argument.begins_with("--screenshot="):
			_screenshot_path = argument.trim_prefix("--screenshot=")


func _build_view() -> void:
	_view = UI._build_view(self, _assign_view_node,
		func(value: String) -> void: _slot_name = value,
		func(value: String) -> void: _character_name = value)


func _assign_view_node(property: String, value: Node) -> void:
	set(property, value)


func _new_game() -> void:
	Storage._new_game(self)


func _materialize_packaged_data() -> String:
	return Storage._materialize_packaged_data(self)


func _save_root() -> String:
	return Storage._save_root(self)


func _slot_path() -> String:
	return Storage._slot_path(self)


func _save_game() -> void:
	Storage._save_game(self)


func _load_game() -> void:
	Storage._load_game(self)


func _snapshot() -> Dictionary:
	var snapshot: Dictionary = _core.get_playable_snapshot()
	if snapshot.has("error"):
		_last_message = "快照錯誤：%s" % snapshot["error"]
	return snapshot


func _refresh() -> void:
	var snapshot := _snapshot()
	if snapshot.has("error"):
		_message.text = _last_message
		return
	var coverage: Dictionary = snapshot["coverage"]
	var residence: int = coverage["residence"]
	_title.text = "AETHERIA — %s" % RESIDENCE_NAMES[residence]
	if residence == 0:
		_map.texture = ImageTexture.create_from_image(Renderer.render(snapshot, LAYERS, _selected_unit_id))
	else:
		var grid: Dictionary = snapshot["site_view"] if residence == 1 else snapshot["local_view"]
		_map.texture = ImageTexture.create_from_image(Grid._render_grid(grid, residence))
	var date: Dictionary = _core.tick_to_date(snapshot["tick"])
	_status.text = "日期：%d 年／季 %d／月 %d／旬 %d　revision=%d　駐留=%s" % [
		date.get("year", 0), date.get("season", 0), date.get("month", 0), date.get("xun", 0),
		snapshot["revision"], RESIDENCE_NAMES[residence],
	]
	_message.text = _last_message
	_coverage_state.text = Format._format_coverage(coverage)
	if residence == 0:
		var target_index: int = snapshot["battle_tile_y"] * snapshot["width"] + snapshot["battle_tile_x"]
		_tile_state.text = "暮橋鎮 (%d,%d)：owner=%d／人口=%d／治安=%d" % [
			snapshot["battle_tile_x"], snapshot["battle_tile_y"], snapshot["owner"][target_index],
			snapshot["population"][target_index], snapshot["order"][target_index],
		]
		_manual_button.visible = snapshot["encounter_pending"]
		_auto_button.visible = snapshot["encounter_pending"]
		_advance_button.disabled = snapshot["encounter_pending"]
		_battle_report.text = Format._format_battle(snapshot["battle_report"])
	_event_log.text = Format._format_events(snapshot["events"])


func _coverage_command(command: String) -> void:
	var before: int = _snapshot()["coverage"]["residence"]
	var result: Dictionary = _core.coverage_command(command)
	if result.has("error"):
		_last_message = "命令被 core 拒絕：%s" % result["error"]
		_refresh()
		return
	_last_message = "core 已完成操作：%s。所有數字已從新快照重讀。" % command
	var after: int = result["coverage"]["residence"]
	if before != after:
		_rebuild_for_residence()
	else:
		_refresh()


func _rebuild_for_residence() -> void:
	if _view != null:
		_view.get_parent().remove_child(_view)
		_view.queue_free()
	_build_view()
	_refresh()


func _map_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT or not event.pressed:
		return
	var snapshot := _snapshot()
	if snapshot.has("error") or snapshot["coverage"]["residence"] != 0 or _map.texture == null:
		return
	var image_size := Vector2(snapshot["width"] * 8, snapshot["height"] * 8)
	var scale := minf(_map.size.x / image_size.x, _map.size.y / image_size.y)
	var origin := (_map.size - image_size * scale) * 0.5
	var image_position: Vector2 = (event.position - origin) / scale
	if image_position.x < 0 or image_position.y < 0 or image_position.x >= image_size.x or image_position.y >= image_size.y:
		return
	var tile := Vector2i(int(image_position.x) / 8, int(image_position.y) / 8)
	if tile.x == snapshot["coverage_tile_x"] and tile.y == snapshot["coverage_tile_y"]:
		_coverage_command("enter_site_manual")
		return
	for unit in snapshot["units"]:
		if unit["player"] and unit["x"] == tile.x and unit["y"] == tile.y:
			_selected_unit_id = unit["id"]
			_last_message = "已選取我軍 #%d（戰力 %d）。" % [unit["id"], unit["power"]]
			_refresh()
			return
	if _selected_unit_id != 0:
		var result: Dictionary = _core.issue_move(_selected_unit_id, tile.x, tile.y)
		_last_message = "移動命令被拒絕：%s" % result["error"] if result.has("error") else "移動意圖已送 core → (%d,%d)。" % [tile.x, tile.y]
		_refresh()


func _select_player() -> void:
	var snapshot := _snapshot()
	_selected_unit_id = snapshot["player_unit_id"]
	_last_message = "已選取青色我軍 #%d。" % _selected_unit_id
	_refresh()


func _move_to_enemy() -> void:
	var snapshot := _snapshot()
	if _selected_unit_id == 0:
		_last_message = "請先選取我軍。"
	else:
		var result: Dictionary = _core.issue_move(_selected_unit_id, snapshot["guided_target_x"], snapshot["guided_target_y"])
		_last_message = "移動命令被拒絕：%s" % result["error"] if result.has("error") else "移動意圖已送 core。"
	_refresh()


func _advance_xun() -> void:
	var result: Dictionary = _core.advance_xun()
	_last_message = "推進被拒絕：%s" % result["error"] if result.has("error") else "一旬完成：七階段=%d、AI=%d。" % [result["stage_count"], result["ai_count"]]
	_refresh()


func _resolve_battle(choice: String) -> void:
	var result: Dictionary = _core.resolve_encounter(choice)
	_last_message = "戰鬥被拒絕：%s" % result["error"] if result.has("error") else "戰鬥完成，世界真值已更新。"
	_refresh()


func _rebuild_view() -> bool:
	var snapshot := _snapshot()
	var layer: int = snapshot["coverage"]["residence"]
	var before := Format._visible_dump(_view)
	_view.get_parent().remove_child(_view)
	_view.queue_free()
	_build_view()
	_refresh()
	var after := Format._visible_dump(_view)
	var matches := before == after
	print("PLAYABLE_REBUILD_LAYER=", RESIDENCE_NAMES[layer], " MATCH=", int(matches))
	return matches


func _run_probe() -> void:
	Probes._run_probe(self)
