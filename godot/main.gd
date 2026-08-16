extends Node


func _ready() -> void:
	var core := AetheriaCore.new()
	add_child(core)
	print("Aetheria core version: ", core.get_core_version())
	print("Tick 0: ", core.tick_to_date(0))
	print("Tick 31104000: ", core.tick_to_date(31_104_000))
	var invalid_date := core.tick_to_date(9_223_372_036_854_775_807)
	assert(invalid_date.is_empty())
	print("Invalid Tick rejected: ", invalid_date.is_empty())
	get_tree().quit()
