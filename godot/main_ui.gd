extends RefCounted


const Controls = preload("res://main_controls.gd")


static func _build_view(owner: Node, assign_node: Callable, slot_changed: Callable, character_changed: Callable) -> Control:
	assign_node.call("_view", MarginContainer.new())
	owner._view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["margin_left", "margin_top", "margin_right", "margin_bottom"]:
		owner._view.add_theme_constant_override(side, 12)
	owner.add_child(owner._view)
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", 12)
	owner._view.add_child(columns)
	var sidebar_scroll := ScrollContainer.new()
	sidebar_scroll.custom_minimum_size.x = 510
	sidebar_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	columns.add_child(sidebar_scroll)
	var sidebar := VBoxContainer.new()
	sidebar.custom_minimum_size.x = 490
	sidebar.add_theme_constant_override("separation", 7)
	sidebar_scroll.add_child(sidebar)
	assign_node.call("_title", Controls._label("AETHERIA — 三層可玩迴圈"))
	owner._title.add_theme_font_size_override("font_size", 24)
	sidebar.add_child(owner._title)
	var inputs := HBoxContainer.new()
	sidebar.add_child(inputs)
	inputs.add_child(Controls._label("Seed"))
	assign_node.call("_seed_edit", LineEdit.new())
	owner._seed_edit.text = "515151"
	owner._seed_edit.custom_minimum_size.x = 105
	inputs.add_child(owner._seed_edit)
	inputs.add_child(Controls._label("Region"))
	assign_node.call("_region_edit", LineEdit.new())
	owner._region_edit.text = "51"
	owner._region_edit.custom_minimum_size.x = 60
	inputs.add_child(owner._region_edit)
	inputs.add_child(Controls._button("開新遊戲", owner._new_game))
	var save_inputs := HBoxContainer.new()
	sidebar.add_child(save_inputs)
	save_inputs.add_child(Controls._label("存檔槽"))
	assign_node.call("_slot_edit", LineEdit.new())
	owner._slot_edit.text = owner._slot_name
	owner._slot_edit.custom_minimum_size.x = 180
	owner._slot_edit.text_changed.connect(slot_changed)
	save_inputs.add_child(owner._slot_edit)
	var character_inputs := HBoxContainer.new()
	sidebar.add_child(character_inputs)
	character_inputs.add_child(Controls._label("角色名"))
	assign_node.call("_character_edit", LineEdit.new())
	owner._character_edit.text = owner._character_name
	owner._character_edit.custom_minimum_size.x = 180
	owner._character_edit.text_changed.connect(character_changed)
	character_inputs.add_child(owner._character_edit)
	character_inputs.add_child(Controls._button("存檔", owner._save_game))
	character_inputs.add_child(Controls._button("讀檔", owner._load_game))
	assign_node.call("_status", Controls._label(""))
	owner._status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(owner._status)
	assign_node.call("_message", Controls._label(""))
	owner._message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	owner._message.modulate = Color8(255, 232, 130)
	sidebar.add_child(owner._message)
	sidebar.add_child(Controls._section("三層操作（親自／系統同一套 core）"))
	var snapshot: Dictionary = owner._snapshot()
	var residence: int = 0
	if not snapshot.has("error"):
		residence = snapshot["coverage"]["residence"]
	Controls._build_coverage_buttons(owner, sidebar, residence)
	var rebuild := Controls._button("free 後從 core 重建本層畫面", owner._rebuild_view)
	rebuild.name = "RebuildButton"
	sidebar.add_child(rebuild)
	assign_node.call("_coverage_state", Controls._label(""))
	owner._coverage_state.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(owner._coverage_state)

	if residence == 0:
		sidebar.add_child(Controls._section("M8.1 戰役仍可玩"))
		var command_buttons := HBoxContainer.new()
		sidebar.add_child(command_buttons)
		command_buttons.add_child(Controls._button("1 選取我軍", owner._select_player))
		command_buttons.add_child(Controls._button("2 移動至敵軍", owner._move_to_enemy))
		assign_node.call("_advance_button", Controls._button("推進一旬", owner._advance_xun))
		sidebar.add_child(owner._advance_button)
		var battle_buttons := HBoxContainer.new()
		sidebar.add_child(battle_buttons)
		assign_node.call("_manual_button", Controls._button("親自指揮（Site）", func() -> void: owner._resolve_battle("manual")))
		battle_buttons.add_child(owner._manual_button)
		assign_node.call("_auto_button", Controls._button("讓系統算（Region）", func() -> void: owner._resolve_battle("auto")))
		battle_buttons.add_child(owner._auto_button)
		sidebar.add_child(Controls._section("遭遇格世界真值"))
		assign_node.call("_tile_state", Controls._label(""))
		sidebar.add_child(owner._tile_state)
		sidebar.add_child(Controls._section("戰報（直接來自 core）"))
		assign_node.call("_battle_report", Controls._label("尚未交戰"))
		owner._battle_report.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		sidebar.add_child(owner._battle_report)
	else:
		assign_node.call("_advance_button", null)
		assign_node.call("_manual_button", null)
		assign_node.call("_auto_button", null)
		assign_node.call("_tile_state", null)
		assign_node.call("_battle_report", null)
	sidebar.add_child(Controls._section("core 事件"))
	assign_node.call("_event_log", Controls._label(""))
	owner._event_log.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sidebar.add_child(owner._event_log)

	assign_node.call("_map", TextureRect.new())
	owner._map.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	owner._map.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	owner._map.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	owner._map.size_flags_vertical = Control.SIZE_EXPAND_FILL
	owner._map.custom_minimum_size = Vector2(680, 540)
	owner._map.mouse_filter = Control.MOUSE_FILTER_STOP
	owner._map.gui_input.connect(owner._map_input)
	columns.add_child(owner._map)
	return owner._view
