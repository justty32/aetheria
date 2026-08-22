extends RefCounted


const Palette = preload("res://region_debug_palette.gd")


static func render(snapshot: Dictionary, layers: Array[bool]) -> Image:
	var width: int = snapshot["width"]
	var height: int = snapshot["height"]
	var image := Image.create(width * Palette.TILE_SIZE, height * Palette.TILE_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Palette.EMPTY)

	var base: PackedInt32Array = snapshot["base"]
	var relief: PackedInt32Array = snapshot["relief"]
	var temperature: PackedByteArray = snapshot["temperature"]
	var moisture: PackedByteArray = snapshot["moisture"]
	var elevation: PackedInt32Array = snapshot["elevation"]
	var owner: PackedInt32Array = snapshot["owner"]
	var terrain_ids: PackedStringArray = snapshot["terrain_ids"]
	var relief_ids: PackedStringArray = snapshot["relief_ids"]
	var maximum_elevation := 0
	for value in elevation:
		maximum_elevation = maxi(maximum_elevation, value)

	for y in height:
		for x in width:
			var index := y * width + x
			var color: Color = Palette.EMPTY
			if layers[0]:
				color = Palette.TERRAIN.get(terrain_ids[base[index]], Color8(255, 0, 255))
			if layers[1]:
				if layers[0]:
					color = Palette.relief_shade(color, relief_ids[relief[index]])
				else:
					color = Palette.relief(relief_ids[relief[index]])
			if layers[2]:
				color = color.lerp(Palette.elevation(elevation[index], maximum_elevation), 0.82)
			if layers[3]:
				color = color.lerp(Palette.temperature(temperature[index]), 0.82)
			if layers[4]:
				color = color.lerp(Palette.moisture(moisture[index]), 0.82)
			if layers[8]:
				var owner_blend := 0.68 if owner[index] != 0 else 0.28
				color = color.lerp(Palette.owner(owner[index]), owner_blend)
			image.fill_rect(Rect2i(x * Palette.TILE_SIZE, y * Palette.TILE_SIZE, Palette.TILE_SIZE, Palette.TILE_SIZE), color)

	if layers[8]:
		_draw_owner_borders(image, snapshot)
	if layers[5]:
		_draw_features(image, snapshot)
	if layers[6]:
		_draw_edges(image, snapshot)
	if layers[7]:
		_draw_settlements(image, snapshot)
	if layers[9]:
		_draw_portals(image, snapshot)
	return image


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
