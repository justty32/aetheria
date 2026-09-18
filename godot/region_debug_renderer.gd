extends RefCounted


const EntityLayers = preload("res://region_debug_layers_entities.gd")
const TerrainLayers = preload("res://region_debug_layers_terrain.gd")
const Palette = preload("res://region_debug_palette.gd")


static func render(snapshot: Dictionary, layers: Array[bool], selected_unit_id: int = 0) -> Image:
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
		TerrainLayers._draw_owner_borders(image, snapshot)
	if layers[5]:
		TerrainLayers._draw_features(image, snapshot)
	if layers[6]:
		TerrainLayers._draw_edges(image, snapshot)
	if layers[7]:
		EntityLayers._draw_settlements(image, snapshot)
	if layers[9]:
		EntityLayers._draw_portals(image, snapshot)
	if snapshot.has("units"):
		EntityLayers._draw_guided_target(image, snapshot)
		EntityLayers._draw_units(image, snapshot, selected_unit_id)
	return image
