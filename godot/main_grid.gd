extends RefCounted




static func _render_grid(grid: Dictionary, residence: int) -> Image:
	var width: int = grid.get("width", 64)
	var height: int = grid.get("height", 64)
	var image := Image.create(width * 8, height * 8, false, Image.FORMAT_RGB8)
	var palette := [Color8(35, 48, 43), Color8(94, 104, 104), Color8(83, 124, 72),
		Color8(143, 121, 91), Color8(96, 164, 220), Color8(255, 220, 90), Color8(225, 105, 65)]
	var cells: PackedByteArray = grid.get("cells", PackedByteArray())
	for index in cells.size():
		var x := index % width
		var y := index / width
		image.fill_rect(Rect2i(x * 8, y * 8, 8, 8), palette[mini(cells[index], palette.size() - 1)])
	if residence >= 2:
		var z: int = grid.get("z", 0)
		image.fill_rect(Rect2i(0, 0, 220, 22), Color8(12, 18, 25))
		# z 數字由左側權威摘要顯示；色塊保留地圖內容可讀性。
		if z < 0:
			image.fill_rect(Rect2i(0, 22, 8, mini(80, image.get_height() - 22)), Color8(110, 76, 145))
	return image
