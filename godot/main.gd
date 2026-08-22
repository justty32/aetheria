extends Node


const Renderer = preload("res://region_debug_renderer.gd")
const LAYERS: Array[bool] = [true, true, false, false, false, true, true, true, true, true]
const RESIDENCE_NAMES := ["Region 大地圖", "Site 城區", "Local 探索", "Local 地城"]
const EVENT_NAMES := [
	"新遊戲已建立", "玩家下達移動意圖", "一旬結算完成", "勢力 AI 已行動",
	"敵軍移動", "發生敵軍遭遇", "戰鬥已結算", "戰後世界已改變",
	"進入 Site", "返回 Region", "城建完成", "進入 Local", "開門並移動",
	"接受湧現任務", "完成湧現任務", "進入負 z 地城", "地城清空",
	"返回 Site", "締結條約", "交給系統處理",
]
const AI_ACTIONS := ["發展", "備戰", "擴張", "宣戰", "提議同盟", "進貢", "統計推進"]
const OUTCOMES := ["戰鬥持續", "我軍潰散", "敵軍潰散", "雙方脫離"]
const FATE_OUTCOMES := ["安然無恙", "負傷", "財產損失", "流離失所", "陣亡"]

var _core: AetheriaCore
var _view: Control
var _map: TextureRect
var _seed_edit: LineEdit
var _region_edit: LineEdit
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
	for side in ["margin_left", "margin_top", "margin_right", "margin_bottom"]:
		_view.add_theme_constant_override(side, 12)
	add_child(_view)
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", 12)
	_view.add_child(columns)
	var sidebar_scroll := ScrollContainer.new()
	sidebar_scroll.custom_minimum_size.x = 510
	sidebar_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	columns.add_child(sidebar_scroll)
	var sidebar := VBoxContainer.new()
	sidebar.custom_minimum_size.x = 490
	sidebar.add_theme_constant_override("separation", 7)
	sidebar_scroll.add_child(sidebar)
	_title = _label("AETHERIA — 三層可玩迴圈")
	_title.add_theme_font_size_override("font_size", 24)
	sidebar.add_child(_title)
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
	_region_edit.custom_minimum_size.x = 60
	inputs.add_child(_region_edit)
	inputs.add_child(_button("開新遊戲", _new_game))
	_status = _label("")
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_status)
	_message = _label("")
	_message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_message.modulate = Color8(255, 232, 130)
	sidebar.add_child(_message)
	sidebar.add_child(_section("三層操作（親自／系統同一套 core）"))
	var snapshot := _snapshot()
	var residence: int = 0
	if not snapshot.has("error"):
		residence = snapshot["coverage"]["residence"]
	_build_coverage_buttons(sidebar, residence)
	var rebuild := _button("free 後從 core 重建本層畫面", _rebuild_view)
	rebuild.name = "RebuildButton"
	sidebar.add_child(rebuild)
	_coverage_state = _label("")
	_coverage_state.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_coverage_state)

	if residence == 0:
		sidebar.add_child(_section("M8.1 戰役仍可玩"))
		var command_buttons := HBoxContainer.new()
		sidebar.add_child(command_buttons)
		command_buttons.add_child(_button("1 選取我軍", _select_player))
		command_buttons.add_child(_button("2 移動至敵軍", _move_to_enemy))
		_advance_button = _button("推進一旬", _advance_xun)
		sidebar.add_child(_advance_button)
		var battle_buttons := HBoxContainer.new()
		sidebar.add_child(battle_buttons)
		_manual_button = _button("親自指揮（Site）", func() -> void: _resolve_battle("manual"))
		battle_buttons.add_child(_manual_button)
		_auto_button = _button("讓系統算（Region）", func() -> void: _resolve_battle("auto"))
		battle_buttons.add_child(_auto_button)
		sidebar.add_child(_section("遭遇格世界真值"))
		_tile_state = _label("")
		sidebar.add_child(_tile_state)
		sidebar.add_child(_section("戰報（直接來自 core）"))
		_battle_report = _label("尚未交戰")
		_battle_report.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		sidebar.add_child(_battle_report)
	else:
		_advance_button = null
		_manual_button = null
		_auto_button = null
		_tile_state = null
		_battle_report = null
	sidebar.add_child(_section("core 事件"))
	_event_log = _label("")
	_event_log.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(_event_log)

	_map = TextureRect.new()
	_map.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_map.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_map.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_map.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_map.custom_minimum_size = Vector2(680, 540)
	_map.mouse_filter = Control.MOUSE_FILTER_STOP
	_map.gui_input.connect(_map_input)
	columns.add_child(_map)


func _build_coverage_buttons(sidebar: VBoxContainer, residence: int) -> void:
	if residence == 0:
		var enter := HBoxContainer.new()
		enter.add_child(_button("親自進 Site", func() -> void: _coverage_command("enter_site_manual")))
		enter.add_child(_button("交給系統：城建", func() -> void: _coverage_command("enter_site_auto")))
		sidebar.add_child(enter)
		var measures := HBoxContainer.new()
		measures.add_child(_button("Site 往返三次", func() -> void: _coverage_command("measure_roundtrips")))
		measures.add_child(_button("N=100 期望值", func() -> void: _coverage_command("measure_calibration")))
		sidebar.add_child(measures)
		sidebar.add_child(_button("締結和平條約", func() -> void: _coverage_command("sign_treaty")))
	elif residence == 1:
		var local_buttons := HBoxContainer.new()
		local_buttons.add_child(_button("親自進 Local", func() -> void: _coverage_command("enter_local_manual")))
		local_buttons.add_child(_button("交給系統：清剿", func() -> void: _coverage_command("enter_local_auto")))
		sidebar.add_child(local_buttons)
		sidebar.add_child(_button("蓋一棟住宅（24 小時）", func() -> void: _coverage_command("build_city")))
		sidebar.add_child(_button("接受『清剿盜匪』湧現任務", func() -> void: _coverage_command("accept_bandit")))
		sidebar.add_child(_button("返回 Region", func() -> void: _coverage_command("leave_site")))
	elif residence == 2:
		var dungeon_buttons := HBoxContainer.new()
		dungeon_buttons.add_child(_button("親自下負 z 地城", func() -> void: _coverage_command("enter_dungeon_manual")))
		dungeon_buttons.add_child(_button("交給系統：清地城", func() -> void: _coverage_command("enter_dungeon_auto")))
		sidebar.add_child(dungeon_buttons)
		sidebar.add_child(_button("開門並向東走一步", func() -> void: _coverage_command("open_door")))
		sidebar.add_child(_button("親自清剿盜匪（完成任務）", func() -> void: _coverage_command("suppress_bandits")))
		sidebar.add_child(_button("返回 Site", func() -> void: _coverage_command("leave_local")))
	else:
		sidebar.add_child(_button("深入下一個負 z 層", func() -> void: _coverage_command("descend_dungeon")))
		sidebar.add_child(_button("觸發機關、擊敗 Boss、清空地城", func() -> void: _coverage_command("clear_dungeon")))
		sidebar.add_child(_button("返回 Local 地表", func() -> void: _coverage_command("leave_dungeon")))


func _label(value: String) -> Label:
	var result := Label.new()
	result.text = value
	return result


func _section(value: String) -> Label:
	var result := _label(value)
	result.add_theme_font_size_override("font_size", 17)
	result.modulate = Color8(145, 205, 255)
	return result


func _button(value: String, callback: Callable) -> Button:
	var result := Button.new()
	result.text = value
	result.pressed.connect(callback)
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
	_last_message = "新遊戲完成。可從 Region 親自進 Site，或交給系統。"
	_rebuild_for_residence()


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
		_map.texture = ImageTexture.create_from_image(_render_grid(grid, residence))
	var date: Dictionary = _core.tick_to_date(snapshot["tick"])
	_status.text = "日期：%d 年／季 %d／月 %d／旬 %d　revision=%d　駐留=%s" % [
		date.get("year", 0), date.get("season", 0), date.get("month", 0), date.get("xun", 0),
		snapshot["revision"], RESIDENCE_NAMES[residence],
	]
	_message.text = _last_message
	_coverage_state.text = _format_coverage(coverage)
	if residence == 0:
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


func _render_grid(grid: Dictionary, residence: int) -> Image:
	var width: int = grid.get("width", 64)
	var height: int = grid.get("height", 64)
	var image := Image.create(width * 8, height * 8, false, Image.FORMAT_RGB8)
	var palette := [Color8(35, 48, 43), Color8(94, 104, 104), Color8(83, 124, 72),
		Color8(143, 121, 91), Color8(96, 164, 220), Color8(255, 220, 90), Color8(225, 105, 65)]
	var cells: PackedByteArray = grid.get("cells", PackedByteArray())
	for index in cells.size():
		var x := index % width
		var y := index / width
		image.fill_rect(Rect2i(x * 8, y * 8, 8, 8), palette[mini(cells[index], palette.size() - 1)])
	if residence >= 2:
		var z: int = grid.get("z", 0)
		image.fill_rect(Rect2i(0, 0, 220, 22), Color8(12, 18, 25))
		# z 數字由左側權威摘要顯示；色塊保留地圖內容可讀性。
		if z < 0:
			image.fill_rect(Rect2i(0, 22, 8, mini(80, image.get_height() - 22)), Color8(110, 76, 145))
	return image


func _format_coverage(value: Dictionary) -> String:
	var hashes: PackedStringArray = value["roundtrip_hashes"]
	var hash_text := "尚未量測" if hashes.is_empty() else " / ".join(hashes)
	var sign := "+" if value["signed_relative_error_percent"] >= 0.0 else ""
	return "霧橋鎮 Region 真值：建設=%d（最近 %d→%d）／治安=%d（最近 %d→%d）／產出=%d\n城建完成=%d；任務=%d（清剿 可用=%s、已接=%s；地城 可用=%s）\n地城：cleared=%s；敵人密度 %d→%d；和平條約=%d\nSite 往返 hash：%s\n親自 vs 系統 N=%d：%d vs %d；相對誤差 %s%.3f%%" % [
		value["development"], value["last_development_before"], value["last_development_after"],
		value["order"], value["last_order_before"], value["last_order_after"], value["production"],
		value["city_buildings"], value["quest_count"], value["bandit_quest_available"],
		value["bandit_quest_accepted"], value["dungeon_quest_available"], value["dungeon_cleared"],
		value["dungeon_density_before"], value["dungeon_density_after"], value["treaty_count"], hash_text,
		value["calibration_n"], value["manual_total"], value["managed_total"], sign,
		value["signed_relative_error_percent"],
	]


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


func _format_battle(report: Dictionary) -> String:
	if report.is_empty():
		return "尚未交戰"
	var before: Dictionary = report["before"]
	var after: Dictionary = report["after"]
	return "%s\n我軍傷亡 %d；敵軍 %d；%s\n%s — %s\n世界：owner %d→%d；人口 %d→%d；治安 %d→%d" % [
		"親自指揮 Site" if report["choice"] == "manual" else "系統計算 Region",
		report["loss_a"], report["loss_b"], OUTCOMES[report["outcome"]], report["named_person"],
		FATE_OUTCOMES[report["named_outcome"]], before["owner"], after["owner"],
		before["population"], after["population"], before["order"], after["order"],
	]


func _format_events(events: Array) -> String:
	var lines := PackedStringArray()
	for index in range(maxi(0, events.size() - 9), events.size()):
		var event: Dictionary = events[index]
		var kind: int = event["kind"]
		var description: String = EVENT_NAMES[kind] if kind < EVENT_NAMES.size() else "事件"
		if kind == 3:
			description += "：勢力 %d／%s" % [event["a"], AI_ACTIONS[event["b"]]]
		elif kind == 5:
			description += "：戰力 %d vs %d" % [event["a"], event["b"]]
		lines.append("#%d %s @(%d,%d)　數字=%d→%d" % [event["id"], description, event["x"], event["y"], event["a"], event["b"]])
	return "\n".join(lines)


func _rebuild_view() -> bool:
	var snapshot := _snapshot()
	var layer: int = snapshot["coverage"]["residence"]
	var before := _visible_dump(_view)
	_view.get_parent().remove_child(_view)
	_view.queue_free()
	_build_view()
	_refresh()
	var after := _visible_dump(_view)
	var matches := before == after
	print("PLAYABLE_REBUILD_LAYER=", RESIDENCE_NAMES[layer], " MATCH=", int(matches))
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
		get_tree().quit(2)
		return
	var snapshot := _snapshot()
	_core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
	_advance_xun()
	_advance_xun()
	_resolve_battle(_mode)
	var rebuild_matches := _rebuild_view()
	var final_snapshot := _snapshot()
	print("PLAYABLE_DEMO_PATH=", _mode)
	print("PLAYABLE_TILE_BLOB_BYTES=", final_snapshot["tile_blob"].size())
	print("PLAYABLE_VISIBLE_DUMP_BEGIN\n", _visible_dump(_view), "\nPLAYABLE_VISIBLE_DUMP_END")
	if not _screenshot_path.is_empty():
		await get_tree().process_frame
		await get_tree().process_frame
		var image := get_viewport().get_texture().get_image()
		var absolute := ProjectSettings.globalize_path(_screenshot_path)
		DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())
		print("PLAYABLE_SCREENSHOT=", absolute, " error=", image.save_png(absolute))
	get_tree().quit(0 if rebuild_matches else 4)


func _run_negative_probe() -> void:
	var before := _snapshot()
	var move: Dictionary = _core.issue_move(before["player_unit_id"], -1, 999)
	var after := _snapshot()
	var invalid_tick: Dictionary = _core.tick_to_date(9223372036854775807)
	var rejected := move.has("error")
	print("BRIDGE_INVALID_COORD_ERROR=", move.get("error", "<accepted>"))
	print("BRIDGE_INVALID_COORD_REVISION=", before["revision"], "->", after["revision"])
	print("BRIDGE_INVALID_TICK_EMPTY=", int(invalid_tick.is_empty()))
	print("BRIDGE_PROCESS_ALIVE=1")
	if _expect_invalid_accepted and rejected:
		push_error("NEGATIVE_CONTROL_RED：預期非法座標被接受，實際錯誤=座標超出 Region 邊界")
		get_tree().quit(1)
		return
	get_tree().quit(0 if rejected and before["revision"] == after["revision"] and invalid_tick.is_empty() else 5)


func _run_performance_probe() -> void:
	var samples: Array[float] = []
	for index in 6:
		_core.new_game(515151, 51)
		var snapshot := _snapshot()
		_core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
		var started := Time.get_ticks_usec()
		var result: Dictionary = _core.advance_xun()
		if result.has("error"):
			get_tree().quit(6)
			return
		if index > 0:
			samples.append(float(Time.get_ticks_usec() - started) / 1000.0)
	print("PLAYABLE_XUN_PERF_WARMUP=1 MEASURED=5 MIN_MS=", samples.min(), " SAMPLES_MS=", samples)
	get_tree().quit(0)
