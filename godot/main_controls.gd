extends RefCounted


static func _build_coverage_buttons(owner: Node, sidebar: VBoxContainer, residence: int) -> void:
	if residence == 0:
		var enter := HBoxContainer.new()
		enter.add_child(_button("親自進 Site", func() -> void: owner._coverage_command("enter_site_manual")))
		enter.add_child(_button("交給系統：城建", func() -> void: owner._coverage_command("enter_site_auto")))
		sidebar.add_child(enter)
		var measures := HBoxContainer.new()
		measures.add_child(_button("Site 往返三次", func() -> void: owner._coverage_command("measure_roundtrips")))
		measures.add_child(_button("N=100 期望值", func() -> void: owner._coverage_command("measure_calibration")))
		sidebar.add_child(measures)
		sidebar.add_child(_button("締結和平條約", func() -> void: owner._coverage_command("sign_treaty")))
	elif residence == 1:
		var local_buttons := HBoxContainer.new()
		local_buttons.add_child(_button("親自進 Local", func() -> void: owner._coverage_command("enter_local_manual")))
		local_buttons.add_child(_button("交給系統：清剿", func() -> void: owner._coverage_command("enter_local_auto")))
		sidebar.add_child(local_buttons)
		sidebar.add_child(_button("蓋一棟住宅（24 小時）", func() -> void: owner._coverage_command("build_city")))
		sidebar.add_child(_button("接受『清剿盜匪』湧現任務", func() -> void: owner._coverage_command("accept_bandit")))
		sidebar.add_child(_button("返回 Region", func() -> void: owner._coverage_command("leave_site")))
	elif residence == 2:
		var dungeon_buttons := HBoxContainer.new()
		dungeon_buttons.add_child(_button("親自下負 z 地城", func() -> void: owner._coverage_command("enter_dungeon_manual")))
		dungeon_buttons.add_child(_button("交給系統：清地城", func() -> void: owner._coverage_command("enter_dungeon_auto")))
		sidebar.add_child(dungeon_buttons)
		sidebar.add_child(_button("開門並向東走一步", func() -> void: owner._coverage_command("open_door")))
		sidebar.add_child(_button("親自清剿盜匪（完成任務）", func() -> void: owner._coverage_command("suppress_bandits")))
		sidebar.add_child(_button("返回 Site", func() -> void: owner._coverage_command("leave_local")))
	else:
		sidebar.add_child(_button("深入下一個負 z 層", func() -> void: owner._coverage_command("descend_dungeon")))
		sidebar.add_child(_button("觸發機關、擊敗 Boss、清空地城", func() -> void: owner._coverage_command("clear_dungeon")))
		sidebar.add_child(_button("返回 Local 地表", func() -> void: owner._coverage_command("leave_dungeon")))


static func _label(value: String) -> Label:
	var result := Label.new()
	result.text = value
	return result


static func _section(value: String) -> Label:
	var result := _label(value)
	result.add_theme_font_size_override("font_size", 17)
	result.modulate = Color8(145, 205, 255)
	return result


static func _button(value: String, callback: Callable) -> Button:
	var result := Button.new()
	result.text = value
	result.pressed.connect(callback)
	return result
