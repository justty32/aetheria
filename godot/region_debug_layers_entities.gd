extends RefCounted


const Palette = preload("res://region_debug_palette.gd")


static func _draw_guided_target(image: Image, snapshot: Dictionary) -> void:
	var x: int = snapshot["guided_target_x"] * Palette.TILE_SIZE
	var y: int = snapshot["guided_target_y"] * Palette.TILE_SIZE
	var color := Color8(255, 230, 60)
	image.fill_rect(Rect2i(x, y, 8, 1), color)
	image.fill_rect(Rect2i(x, y + 7, 8, 1), color)
	image.fill_rect(Rect2i(x, y, 1, 8), color)
	image.fill_rect(Rect2i(x + 7, y, 1, 8), color)


static func _draw_units(image: Image, snapshot: Dictionary, selected_unit_id: int) -> void:
	for unit in snapshot["units"]:
		var x: int = unit["x"] * Palette.TILE_SIZE
		var y: int = unit["y"] * Palette.TILE_SIZE
		var color := Color8(70, 220, 255) if unit["player"] else Color8(255, 72, 72)
		image.fill_rect(Rect2i(x + 2, y + 2, 4, 4), color)
		if unit["id"] == selected_unit_id:
			image.fill_rect(Rect2i(x, y, 8, 1), Color.WHITE)
			image.fill_rect(Rect2i(x, y + 7, 8, 1), Color.WHITE)
			image.fill_rect(Rect2i(x, y, 1, 8), Color.WHITE)
			image.fill_rect(Rect2i(x + 7, y, 1, 8), Color.WHITE)


static func _draw_settlements(image: Image, snapshot: Dictionary) -> void:
	var width: int = snapshot["width"]
	var settlement: PackedInt32Array = snapshot["settlement"]
	for index in settlement.size():
		var tier := settlement[index]
		if tier == 0:
			continue
		var size := tier * 2 + 1
		var x := (index % width) * 8 + (8 - size) / 2
		var y := (index / width) * 8 + (8 - size) / 2
		image.fill_rect(Rect2i(x, y, size, size), Palette.SETTLEMENT[tier])


static func _draw_portals(image: Image, snapshot: Dictionary) -> void:
	var portals: Dictionary = snapshot["portals"]
	var portal_x: PackedInt32Array = portals["x"]
	var portal_y: PackedInt32Array = portals["y"]
	var portal_type: PackedInt32Array = portals["type"]
	for index in portal_x.size():
		var x := portal_x[index] * 8
		var y := portal_y[index] * 8
		var type := portal_type[index]
		var color: Color = Palette.PORTAL.get(type, Color8(255, 0, 255))
		if type == 0: # Sea lane: square.
			image.fill_rect(Rect2i(x + 1, y + 1, 6, 2), color)
			image.fill_rect(Rect2i(x + 1, y + 5, 6, 2), color)
			image.fill_rect(Rect2i(x + 1, y + 1, 2, 6), color)
			image.fill_rect(Rect2i(x + 5, y + 1, 2, 6), color)
		elif type == 1: # Pass: triangle.
			image.fill_rect(Rect2i(x + 3, y + 1, 2, 2), color)
			image.fill_rect(Rect2i(x + 2, y + 3, 4, 2), color)
			image.fill_rect(Rect2i(x + 1, y + 5, 6, 2), color)
		elif type == 2: # Underground: X.
			image.fill_rect(Rect2i(x + 1, y + 1, 2, 2), color)
			image.fill_rect(Rect2i(x + 5, y + 1, 2, 2), color)
			image.fill_rect(Rect2i(x + 3, y + 3, 2, 2), color)
			image.fill_rect(Rect2i(x + 1, y + 5, 2, 2), color)
			image.fill_rect(Rect2i(x + 5, y + 5, 2, 2), color)
		else: # Teleport: cross.
			image.fill_rect(Rect2i(x + 3, y + 1, 2, 6), color)
			image.fill_rect(Rect2i(x + 1, y + 3, 6, 2), color)
