extends RefCounted


const RAW_FILES := [
	"attributes.toml", "biomes.toml", "civilization.toml", "combat.toml",
	"damage.toml", "diplomacy.toml", "dungeon.toml", "edges.toml",
	"feature.toml", "ground.toml", "local_buildings.toml", "movement.toml",
	"power.toml", "power_sources.toml", "relief.toml", "site_build.toml",
	"site_city.toml", "site_projection.toml", "site_wild.toml", "terrain.toml",
	"world_graph.toml", "world_observations.toml",
]

static func _new_game(owner: Node) -> void:
	if not owner._seed_edit.text.is_valid_int() or not owner._region_edit.text.is_valid_int():
		owner._last_message = "錯誤：Seed 與 Region 必須是整數。"
		owner._message.text = owner._last_message
		return
	var data_path: String = owner._materialize_packaged_data()
	if data_path.is_empty():
		owner._last_message = "錯誤：無法準備 exported build 內附的基準 data。"
		owner._message.text = owner._last_message
		return
	var result: Dictionary = owner._core.new_game(
		owner._seed_edit.text.to_int(), owner._region_edit.text.to_int(), data_path)
	if result.has("error"):
		owner._last_message = "錯誤：%s" % result["error"]
		owner._message.text = owner._last_message
		return
	owner._selected_unit_id = 0
	owner._last_message = "新遊戲完成。可從 Region 親自進 Site，或交給系統。"
	owner._rebuild_for_residence()


static func _materialize_packaged_data(owner: Node) -> String:
	# res:// 可能位於 exported PCK，C++ filesystem 無法直接讀；先逐位元組
	# materialize 到 user://，再把絕對路徑跨 bridge 傳入。
	var destination := ProjectSettings.globalize_path("user://packaged-data")
	if DirAccess.make_dir_recursive_absolute(destination) != OK:
		return ""
	for file_name: String in RAW_FILES:
		var source := "res://data/" + file_name
		if not FileAccess.file_exists(source):
			return ""
		var bytes := FileAccess.get_file_as_bytes(source)
		var output := FileAccess.open(destination.path_join(file_name), FileAccess.WRITE)
		if output == null:
			return ""
		output.store_buffer(bytes)
		output.close()
	return destination


static func _save_root(owner: Node) -> String:
	return ProjectSettings.globalize_path("user://saves")


static func _slot_path(owner: Node) -> String:
	var name: String = owner._slot_edit.text.strip_edges()
	if name.is_empty() or not name.is_valid_filename() or name == "." or name == "..":
		return ""
	owner._slot_name = name
	return owner._save_root().path_join(name)


static func _save_game(owner: Node) -> void:
	var path: String = owner._slot_path()
	var character: String = owner._character_edit.text.strip_edges()
	if path.is_empty() or character.is_empty() or not character.is_valid_filename() or character == "." or character == "..":
		owner._last_message = "錯誤：存檔槽名與角色名皆須為不含路徑字元的非空名稱。"
		owner._message.text = owner._last_message
		return
	owner._character_name = character
	DirAccess.make_dir_recursive_absolute(owner._save_root())
	var result: Dictionary = owner._core.save_game(path, character)
	if result.has("error"):
		owner._last_message = "存檔失敗：%s" % result["error"]
	else:
		var saves: PackedStringArray = owner._core.list_saves(owner._save_root())
		var characters: PackedStringArray = owner._core.list_characters(path)
		owner._last_message = "存檔成功：%s / %s（角色：%s；現有槽：%s）" % [owner._slot_name, character, ", ".join(characters), ", ".join(saves)]
	owner._refresh()


static func _load_game(owner: Node) -> void:
	var path: String = owner._slot_path()
	var character: String = owner._character_edit.text.strip_edges()
	if path.is_empty() or character.is_empty() or not character.is_valid_filename() or character == "." or character == "..":
		owner._last_message = "錯誤：存檔槽名與角色名皆須為不含路徑字元的非空名稱。"
		owner._message.text = owner._last_message
		return
	owner._character_name = character
	var result: Dictionary = owner._core.load_game(path, character)
	if result.has("error"):
		owner._last_message = "讀檔失敗：%s" % result["error"]
		owner._message.text = owner._last_message
		return
	owner._selected_unit_id = 0
	owner._last_message = "讀檔成功：%s / %s。世界與角色狀態已由磁碟冷讀重建。" % [owner._slot_name, character]
	owner._rebuild_for_residence()
