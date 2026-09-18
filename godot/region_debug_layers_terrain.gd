extends RefCounted


const Palette = preload("res://region_debug_palette.gd")


static func _draw_owner_borders(image: Image, snapshot: Dictionary) -> void:
	var width: int = snapshot["width"]
	var height: int = snapshot["height"]
	var owner: PackedInt32Array = snapshot["owner"]
	for y in height:
		for x in width:
			var index := y * width + x
			if x + 1 < width and owner[index] != owner[index + 1]:
				image.fill_rect(Rect2i((x + 1) * 8 - 1, y * 8, 2, 8), Palette.OWNER_BORDER)
			if y + 1 < height and owner[index] != owner[index + width]:
				image.fill_rect(Rect2i(x * 8, (y + 1) * 8 - 1, 8, 2), Palette.OWNER_BORDER)


static func _draw_features(image: Image, snapshot: Dictionary) -> void:
	var width: int = snapshot["width"]
	var feature: PackedInt32Array = snapshot["feature"]
	var feature_ids: PackedStringArray = snapshot["feature_ids"]
	for index in feature.size():
		var feature_id := feature_ids[feature[index]]
		if feature_id == "feature.none":
			continue
		var color: Color = Palette.FEATURE.get(feature_id, Color8(255, 0, 255))
		var x := (index % width) * Palette.TILE_SIZE
		var y := (index / width) * Palette.TILE_SIZE
		if feature_id == "feature.landmark":
			image.fill_rect(Rect2i(x + 3, y + 1, 2, 6), color)
			image.fill_rect(Rect2i(x + 1, y + 3, 6, 2), color)
			continue
		if feature_id == "feature.ancient_foundation":
			image.fill_rect(Rect2i(x + 1, y + 1, 6, 1), color)
			image.fill_rect(Rect2i(x + 1, y + 6, 6, 1), color)
			image.fill_rect(Rect2i(x + 1, y + 1, 1, 6), color)
			image.fill_rect(Rect2i(x + 6, y + 1, 1, 6), color)
			continue
		var size := 4
		if feature_id == "feature.ruin_village":
			size = 2
		elif feature_id == "feature.ruin_city":
			size = 6
		image.fill_rect(Rect2i(x + (8 - size) / 2, y + (8 - size) / 2, size, size), color)


static func _draw_edges(image: Image, snapshot: Dictionary) -> void:
	var width: int = snapshot["width"]
	var height: int = snapshot["height"]
	var edges: PackedInt32Array = snapshot["edges"]
	var edge_ids: PackedStringArray = snapshot["edge_ids"]
	for y in height:
		for x in width:
			var index := y * width + x
			if x + 1 < width:
				_draw_edge(image, edge_ids[edges[index * 4 + 1]], (x + 1) * 8, y * 8, true)
			if y + 1 < height:
				_draw_edge(image, edge_ids[edges[index * 4 + 2]], x * 8, (y + 1) * 8, false)


static func _draw_edge(image: Image, edge_id: String, x: int, y: int, vertical: bool) -> void:
	var style := _edge_style(edge_id)
	if style.is_empty():
		return
	var color: Color = style[0]
	var thickness: int = style[1]
	var crosses_boundary: bool = style[2]
	if vertical and crosses_boundary:
		image.fill_rect(Rect2i(x - 4, y + 4 - thickness / 2, 9, thickness), color)
	elif vertical:
		image.fill_rect(Rect2i(x - thickness / 2, y, thickness, 8), color)
	elif crosses_boundary:
		image.fill_rect(Rect2i(x + 4 - thickness / 2, y - 4, thickness, 9), color)
	else:
		image.fill_rect(Rect2i(x, y - thickness / 2, 8, thickness), color)


static func _edge_style(edge_id: String) -> Array:
	if edge_id.contains("bridge") or edge_id.contains("ford") or edge_id.contains("ferry"):
		return [Palette.EDGE["crossing"], 3, true]
	if edge_id == "edge.ancient_road":
		return [Palette.EDGE["ancient"], 2, true]
	if edge_id == "edge.great_river":
		return [Palette.EDGE["great_river"], 3, false]
	if edge_id == "edge.river":
		return [Palette.EDGE["river"], 2, false]
	if edge_id == "edge.stream":
		return [Palette.EDGE["stream"], 1, false]
	if edge_id == "edge.highway":
		return [Palette.EDGE["highway"], 3, true]
	if edge_id == "edge.road":
		return [Palette.EDGE["road"], 2, true]
	if edge_id == "edge.trail":
		return [Palette.EDGE["trail"], 1, true]
	return []
