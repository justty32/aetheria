extends Node


const Renderer = preload("res://region_debug_renderer.gd")
const LAYERS: Array[bool] = [true, true, false, false, false, true, true, true, true, true]
const EVENT_NAMES := [
	"新遊戲已建立", "玩家下達移動意圖", "一旬結算完成", "勢力 AI 已行動",
	"敵軍移動", "發生敵軍遭遇", "戰鬥已結算", "戰後世界已改變",
]
const AI_ACTIONS := ["發展", "備戰", "擴張", "宣戰", "提議同盟", "進貢", "統計推進"]
const OUTCOMES := ["戰鬥持續", "我軍潰散", "敵軍潰散", "雙方脫離"]
const FATE_OUTCOMES := ["安然無恙", "負傷", "財產損失", "流離失所", "陣亡"]

var _core: AetheriaCore
var _view: Control
var _map: TextureRect
var _seed_edit: LineEdit
var _region_edit: LineEdit
var _status: Label
var _message: Label
var _tile_state: Label
var _battle_report: Label
var _event_log: Label
var _advance_button: Button
var _manual_button: Button
var _auto_button: Button
var _selected_unit_id := 0
var _last_message := "請先點青色方塊選取我軍，再點紅色敵軍下達移動命令。"
var _mode := ""
var _screenshot_path := ""
var _expect_invalid_accepted := false


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
	_view = MarginContainer.new()
	_view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_view.add_theme_constant_override("margin_left", 12)
	_view.add_theme_constant_override("margin_top", 12)
	_view.add_theme_constant_override("margin_right", 12)
	_view.add_theme_constant_override("margin_bottom", 12)
	add_child(_view)
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", 12)
	_view.add_child(columns)

	var sidebar_scroll := ScrollContainer.new()
	sidebar_scroll.custom_minimum_size.x = 390
	sidebar_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	columns.add_child(sidebar_scroll)
	var sidebar := VBoxContainer.new()
	sidebar.custom_minimum_size.x = 370
	sidebar.add_theme_constant_override("separation", 8)
	sidebar_scroll.add_child(sidebar)
	var title := _label("AETHERIA — 可玩戰役迴圈")
	title.add_theme_font_size_override("font_size", 24)
	sidebar.add_child(title)

	var inputs := HBoxContainer.new()
	sidebar.add_child(inputs)
	inputs.add_child(_label("Seed"))
	_seed_edit = LineEdit.new()
	_seed_edit.text = "515151"
	_seed_edit.custom_minimum_size.x = 105
	inputs.add_child(_seed_edit)
	inputs.add_child(_label("Region"))
	_region_edit = LineEdit.new()
	_region_edit.text = "51"
	_region_edit.custom_minimum_size.x = 65
	inputs.add_child(_region_edit)
	var new_button := Button.new()
	new_button.text = "開新遊戲"
	new_button.pressed.connect(_new_game)
	inputs.add_child(new_button)

	var help := _label("操作：①選取我軍 ②下達移動意圖 ③按「推進一旬」兩次 ④遭遇後選擇親自指揮或系統計算。也可直接在地圖點青色我軍與紅色敵軍。")
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(help)
	_status = _label("")
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_status)
	_message = _label("")
	_message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_message.modulate = Color8(255, 232, 130)
	sidebar.add_child(_message)

	var command_buttons := HBoxContainer.new()
	sidebar.add_child(command_buttons)
	var select_button := Button.new()
	select_button.text = "1 選取我軍"
	select_button.pressed.connect(_select_player)
	command_buttons.add_child(select_button)
	var move_button := Button.new()
	move_button.text = "2 移動至敵軍"
	move_button.pressed.connect(_move_to_enemy)
	command_buttons.add_child(move_button)
	_advance_button = Button.new()
	_advance_button.text = "推進一旬"
	_advance_button.pressed.connect(_advance_xun)
	sidebar.add_child(_advance_button)
	var battle_buttons := HBoxContainer.new()
	sidebar.add_child(battle_buttons)
	_manual_button = Button.new()
	_manual_button.text = "親自指揮（Site）"
	_manual_button.pressed.connect(func() -> void: _resolve_battle("manual"))
	battle_buttons.add_child(_manual_button)
	_auto_button = Button.new()
	_auto_button.text = "讓系統算（Region）"
	_auto_button.pressed.connect(func() -> void: _resolve_battle("auto"))
	battle_buttons.add_child(_auto_button)
	var rebuild_button := Button.new()
	rebuild_button.text = "free 後從 core 重建整個畫面"
	rebuild_button.pressed.connect(_rebuild_view)
	sidebar.add_child(rebuild_button)

	sidebar.add_child(_section("遭遇格世界真值"))
	_tile_state = _label("")
	sidebar.add_child(_tile_state)
	sidebar.add_child(_section("戰報（直接來自 core）"))
	_battle_report = _label("尚未交戰")
	_battle_report.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_battle_report)
	sidebar.add_child(_section("core 事件"))
	_event_log = _label("")
	_event_log.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_event_log)

	_map = TextureRect.new()
	_map.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_map.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_map.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_map.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_map.custom_minimum_size = Vector2(720, 540)
	_map.mouse_filter = Control.MOUSE_FILTER_STOP
	_map.gui_input.connect(_map_input)
	columns.add_child(_map)


func _label(value: String) -> Label:
	var result := Label.new()
	result.text = value
	return result


func _section(value: String) -> Label:
	var result := _label(value)
	result.add_theme_font_size_override("font_size", 17)
	result.modulate = Color8(145, 205, 255)
	return result


func _new_game() -> void:
	if not _seed_edit.text.is_valid_int() or not _region_edit.text.is_valid_int():
		_last_message = "錯誤：Seed 與 Region 必須是整數。"
		_message.text = _last_message
		return
	var result: Dictionary = _core.new_game(_seed_edit.text.to_int(), _region_edit.text.to_int())
	if result.has("error"):
		_last_message = "錯誤：%s" % result["error"]
		_message.text = _last_message
		return
	_selected_unit_id = 0
	_last_message = "新遊戲完成。請按「1 選取我軍」。"
	_refresh()


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
	var image := Renderer.render(snapshot, LAYERS, _selected_unit_id)
	_map.texture = ImageTexture.create_from_image(image)
	var date: Dictionary = _core.tick_to_date(snapshot["tick"])
	_status.text = "日期：%d 年／季 %d／月 %d／旬 %d　revision=%d\n批次拉圖：%d bytes（PackedByteArray）；單次 bridge 呼叫" % [
		date.get("year", 0), date.get("season", 0), date.get("month", 0), date.get("xun", 0),
		snapshot["revision"], snapshot["batch_bytes"],
	]
	_message.text = _last_message
	var target_index: int = snapshot["battle_tile_y"] * snapshot["width"] + snapshot["battle_tile_x"]
	_tile_state.text = "暮橋鎮 (%d,%d)：owner=%d／人口=%d／治安=%d" % [
		snapshot["battle_tile_x"], snapshot["battle_tile_y"], snapshot["owner"][target_index],
		snapshot["population"][target_index], snapshot["order"][target_index],
	]
	_manual_button.visible = snapshot["encounter_pending"]
	_auto_button.visible = snapshot["encounter_pending"]
	_advance_button.disabled = snapshot["encounter_pending"]
	_battle_report.text = _format_battle(snapshot["battle_report"])
	_event_log.text = _format_events(snapshot["events"])


func _map_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or event.button_index != MOUSE_BUTTON_LEFT or not event.pressed:
		return
	var snapshot := _snapshot()
	if snapshot.has("error") or _map.texture == null:
		return
	var image_size := Vector2(snapshot["width"] * 8, snapshot["height"] * 8)
	var scale := minf(_map.size.x / image_size.x, _map.size.y / image_size.y)
	var origin := (_map.size - image_size * scale) * 0.5
	var image_position: Vector2 = (event.position - origin) / scale
	if image_position.x < 0 or image_position.y < 0 or image_position.x >= image_size.x or image_position.y >= image_size.y:
		return
	var tile := Vector2i(int(image_position.x) / 8, int(image_position.y) / 8)
	for unit in snapshot["units"]:
		if unit["player"] and unit["x"] == tile.x and unit["y"] == tile.y:
			_selected_unit_id = unit["id"]
			_last_message = "已選取我軍 #%d（戰力 %d）。現在點目的地。" % [unit["id"], unit["power"]]
			_refresh()
			return
	if _selected_unit_id == 0:
		_last_message = "請先點青色我軍。"
		_refresh()
		return
	var result: Dictionary = _core.issue_move(_selected_unit_id, tile.x, tile.y)
	if result.has("error"):
		_last_message = "移動命令被拒絕：%s" % result["error"]
	else:
		_last_message = "移動意圖已送 core：部隊 #%d → (%d,%d)。按「推進一旬」。" % [_selected_unit_id, tile.x, tile.y]
	_refresh()


func _select_player() -> void:
	var snapshot := _snapshot()
	if snapshot.has("error"):
		return
	_selected_unit_id = snapshot["player_unit_id"]
	_last_message = "已選取青色我軍 #%d。接著按「2 移動至敵軍」。" % _selected_unit_id
	_refresh()


func _move_to_enemy() -> void:
	var snapshot := _snapshot()
	if snapshot.has("error"):
		return
	if _selected_unit_id == 0:
		_last_message = "請先按「1 選取我軍」。"
		_refresh()
		return
	var result: Dictionary = _core.issue_move(_selected_unit_id, snapshot["guided_target_x"], snapshot["guided_target_y"])
	if result.has("error"):
		_last_message = "移動命令被拒絕：%s" % result["error"]
	else:
		_last_message = "移動意圖已送 core：部隊 #%d → 敵軍所在格 (%d,%d)。" % [
			_selected_unit_id, snapshot["guided_target_x"], snapshot["guided_target_y"],
		]
	_refresh()


func _advance_xun() -> void:
	var result: Dictionary = _core.advance_xun()
	if result.has("error"):
		_last_message = "推進被拒絕：%s" % result["error"]
	else:
		_last_message = "一旬完成：七階段=%d、AI 行動=%d、牆鐘 %.3f ms。" % [result["stage_count"], result["ai_count"], result["elapsed_ms"]]
		if result["encounter_pending"]:
			_last_message += " 遭遇敵軍！請選擇親自指揮或讓系統計算。"
	_refresh()


func _resolve_battle(choice: String) -> void:
	var result: Dictionary = _core.resolve_encounter(choice)
	if result.has("error"):
		_last_message = "戰鬥結算被拒絕：%s" % result["error"]
	else:
		_last_message = "戰鬥已結算；戰報與暮橋鎮的新 owner／人口／治安如下。"
	_refresh()


func _format_battle(report: Dictionary) -> String:
	if report.is_empty():
		return "尚未交戰"
	var before: Dictionary = report["before"]
	var after: Dictionary = report["after"]
	var path := "親自指揮 Site" if report["choice"] == "manual" else "系統計算 Region"
	return "%s\n我軍傷亡 %d（期望 %d），敵軍傷亡 %d（期望 %d）\n結果：%s；士氣 Δ=%d/%d\n具名者：%s — %s\n世界：owner %d→%d；人口 %d→%d；治安 %d→%d\n來源：%s + %s" % [
		path, report["loss_a"], report["region_expected_loss_a"], report["loss_b"], report["region_expected_loss_b"],
		OUTCOMES[report["outcome"]], report["morale_delta_a"], report["morale_delta_b"], report["named_person"],
		FATE_OUTCOMES[report["named_outcome"]], before["owner"], after["owner"], before["population"], after["population"],
		before["order"], after["order"], report["combat_result_source"], report["layer_result_source"],
	]


func _format_events(events: Array) -> String:
	var lines := PackedStringArray()
	var first := maxi(0, events.size() - 10)
	for index in range(first, events.size()):
		var event: Dictionary = events[index]
		var description: String = EVENT_NAMES[event["kind"]]
		if event["kind"] == 3:
			description += "：勢力 %d／%s" % [event["a"], AI_ACTIONS[event["b"]]]
		elif event["kind"] == 4:
			description += "：x %d→%d" % [event["a"], event["b"]]
		elif event["kind"] == 5:
			description += "：戰力 %d vs %d" % [event["a"], event["b"]]
		lines.append("#%d %s @(%d,%d)" % [event["id"], description, event["x"], event["y"]])
	return "\n".join(lines)


func _rebuild_view() -> bool:
	var before := _visible_dump(_view)
	_view.free()
	_build_view()
	_refresh()
	var after := _visible_dump(_view)
	var matches := before == after
	print("PLAYABLE_REBUILD_BEFORE_BEGIN\n", before, "\nPLAYABLE_REBUILD_BEFORE_END")
	print("PLAYABLE_REBUILD_AFTER_BEGIN\n", after, "\nPLAYABLE_REBUILD_AFTER_END")
	print("PLAYABLE_REBUILD_MATCH=", int(matches))
	return matches


func _visible_dump(node: Node) -> String:
	var lines := PackedStringArray()
	_collect_visible_text(node, lines)
	return "\n".join(lines)


func _collect_visible_text(node: Node, lines: PackedStringArray) -> void:
	if node is CanvasItem and not node.visible:
		return
	if node is Label:
		lines.append(node.text)
	elif node is Button:
		lines.append("[%s]" % node.text)
	for child in node.get_children():
		_collect_visible_text(child, lines)


func _run_probe() -> void:
	if _mode == "negative":
		_run_negative_probe()
		return
	if _mode == "performance":
		_run_performance_probe()
		return
	if _mode != "manual" and _mode != "auto":
		push_error("未知 playable demo：%s" % _mode)
		get_tree().quit(2)
		return
	var snapshot := _snapshot()
	var command: Dictionary = _core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
	if command.has("error"):
		push_error(command["error"])
		get_tree().quit(3)
		return
	_advance_xun()
	_advance_xun()
	_resolve_battle(_mode)
	var rebuild_matches := _rebuild_view()
	var final_snapshot := _snapshot()
	print("PLAYABLE_DEMO_PATH=", _mode)
	print("PLAYABLE_TILE_BLOB_BYTES=", final_snapshot["tile_blob"].size())
	print("PLAYABLE_BATCH_MS=", final_snapshot["batch_ms"])
	print("PLAYABLE_VISIBLE_DUMP_BEGIN\n", _visible_dump(_view), "\nPLAYABLE_VISIBLE_DUMP_END")
	if not _screenshot_path.is_empty():
		await get_tree().process_frame
		await get_tree().process_frame
		var image := get_viewport().get_texture().get_image()
		var absolute := ProjectSettings.globalize_path(_screenshot_path)
		DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())
		var error := image.save_png(absolute)
		print("PLAYABLE_SCREENSHOT=", absolute, " error=", error)
	get_tree().quit(0 if rebuild_matches else 4)


func _run_negative_probe() -> void:
	var before := _snapshot()
	var move: Dictionary = _core.issue_move(before["player_unit_id"], -1, 999)
	var after := _snapshot()
	var ocean_id: int = before["terrain_ids"].find("terrain.ocean")
	var ocean_index: int = before["base"].find(ocean_id)
	var core_before_revision: int = after["revision"]
	var core_move: Dictionary = _core.issue_move(
		before["player_unit_id"], ocean_index % before["width"], floori(float(ocean_index) / before["width"]))
	var core_after := _snapshot()
	var invalid_tick: Dictionary = _core.tick_to_date(9223372036854775807)
	var rejected: bool = move.has("error")
	var unchanged: bool = before["revision"] == after["revision"]
	var core_rejected: bool = core_move.has("error")
	var core_unchanged: bool = core_before_revision == core_after["revision"]
	print("BRIDGE_INVALID_COORD_ERROR=", move.get("error", "<accepted>"))
	print("BRIDGE_INVALID_COORD_REJECTED=", int(rejected))
	print("BRIDGE_INVALID_COORD_REVISION=", before["revision"], "->", after["revision"])
	print("BRIDGE_INVALID_TICK_EMPTY=", int(invalid_tick.is_empty()))
	print("CORE_ILLEGAL_MOVE_ERROR=", core_move.get("error", "<accepted>"))
	print("CORE_ILLEGAL_MOVE_REJECTED=", int(core_rejected))
	print("CORE_ILLEGAL_MOVE_REVISION=", core_before_revision, "->", core_after["revision"])
	print("BRIDGE_PROCESS_ALIVE=1")
	if _expect_invalid_accepted:
		if rejected:
			push_error("NEGATIVE_CONTROL_RED：預期非法座標被接受，實際錯誤=座標超出 Region 邊界")
			get_tree().quit(1)
			return
	get_tree().quit(0 if rejected and unchanged and invalid_tick.is_empty() and core_rejected and core_unchanged else 5)


func _run_performance_probe() -> void:
	var samples: Array[float] = []
	for index in 6:
		_core.new_game(515151, 51)
		var snapshot := _snapshot()
		_core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
		var started := Time.get_ticks_usec()
		var result: Dictionary = _core.advance_xun()
		var elapsed := float(Time.get_ticks_usec() - started) / 1000.0
		if result.has("error"):
			push_error(result["error"])
			get_tree().quit(6)
			return
		if index > 0:
			samples.append(elapsed)
	print("PLAYABLE_XUN_PERF_WARMUP=1 MEASURED=5 MIN_MS=", samples.min(), " SAMPLES_MS=", samples)
	get_tree().quit(0)
