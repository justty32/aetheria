extends PanelContainer


var _lines: VBoxContainer


func render(core: AetheriaCore) -> void:
	for child in get_children():
		child.free()
	_lines = VBoxContainer.new()
	add_child(_lines)
	_lines.add_child(_label(tr("ui.event_panel_title")))
	for event in core.poll_events():
		_lines.add_child(_label(_localized_text(event["heading"])))
		for line in event["lines"]:
			_lines.add_child(_label(_localized_text(line)))


func dump_text() -> String:
	var result: PackedStringArray
	for child in _lines.get_children():
		if child is Label:
			result.append(child.text)
	return "\n".join(result)


func _localized_text(message: Dictionary) -> String:
	var values := {}
	var parameters: Dictionary = message["params"]
	for name in parameters:
		var argument: Dictionary = parameters[name]
		values[name] = tr(argument["value"]) if argument["i18n"] else argument["value"]
	return tr(message["key"]).format(values)


func _label(text: String) -> Label:
	var label := Label.new()
	label.text = text
	return label
