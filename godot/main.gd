extends Node


func _ready() -> void:
	var core := AetheriaCore.new()
	add_child(core)
	print("Aetheria core version: ", core.get_core_version())
	print("Tick 0: ", core.tick_to_date(0))
	print("Tick 31104000: ", core.tick_to_date(31_104_000))
	get_tree().quit()

