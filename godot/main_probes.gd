extends RefCounted


const Format = preload("res://main_format.gd")


static func _run_probe(owner: Node) -> void:
	if owner._mode == "negative":
		_run_negative_probe(owner)
		return
	if owner._mode == "performance":
		_run_performance_probe(owner)
		return
	if owner._mode != "manual" and owner._mode != "auto":
		owner.get_tree().quit(2)
		return
	var snapshot: Dictionary = owner._snapshot()
	owner._core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
	owner._advance_xun()
	owner._advance_xun()
	owner._resolve_battle(owner._mode)
	var rebuild_matches: bool = owner._rebuild_view()
	var final_snapshot: Dictionary = owner._snapshot()
	print("PLAYABLE_DEMO_PATH=", owner._mode)
	print("PLAYABLE_TILE_BLOB_BYTES=", final_snapshot["tile_blob"].size())
	print("PLAYABLE_VISIBLE_DUMP_BEGIN\n", Format._visible_dump(owner._view), "\nPLAYABLE_VISIBLE_DUMP_END")
	if not owner._screenshot_path.is_empty():
		await owner.get_tree().process_frame
		await owner.get_tree().process_frame
		var image := owner.get_viewport().get_texture().get_image()
		var absolute := ProjectSettings.globalize_path(owner._screenshot_path)
		DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())
		print("PLAYABLE_SCREENSHOT=", absolute, " error=", image.save_png(absolute))
	owner.get_tree().quit(0 if rebuild_matches else 4)


static func _run_negative_probe(owner: Node) -> void:
	var before: Dictionary = owner._snapshot()
	var move: Dictionary = owner._core.issue_move(before["player_unit_id"], -1, 999)
	var after: Dictionary = owner._snapshot()
	var invalid_tick: Dictionary = owner._core.tick_to_date(9223372036854775807)
	var rejected := move.has("error")
	print("BRIDGE_INVALID_COORD_ERROR=", move.get("error", "<accepted>"))
	print("BRIDGE_INVALID_COORD_REVISION=", before["revision"], "->", after["revision"])
	print("BRIDGE_INVALID_TICK_EMPTY=", int(invalid_tick.is_empty()))
	print("BRIDGE_PROCESS_ALIVE=1")
	if owner._expect_invalid_accepted and rejected:
		push_error("NEGATIVE_CONTROL_RED：預期非法座標被接受，實際錯誤=座標超出 Region 邊界")
		owner.get_tree().quit(1)
		return
	owner.get_tree().quit(0 if rejected and before["revision"] == after["revision"] and invalid_tick.is_empty() else 5)


static func _run_performance_probe(owner: Node) -> void:
	var samples: Array[float] = []
	for index in 6:
		owner._core.new_game(515151, 51, owner._materialize_packaged_data())
		var snapshot: Dictionary = owner._snapshot()
		owner._core.issue_move(snapshot["player_unit_id"], snapshot["guided_target_x"], snapshot["guided_target_y"])
		var started := Time.get_ticks_usec()
		var result: Dictionary = owner._core.advance_xun()
		if result.has("error"):
			owner.get_tree().quit(6)
			return
		if index > 0:
			samples.append(float(Time.get_ticks_usec() - started) / 1000.0)
	print("PLAYABLE_XUN_PERF_WARMUP=1 MEASURED=5 MIN_MS=", samples.min(), " SAMPLES_MS=", samples)
	owner.get_tree().quit(0)
