extends RefCounted


const TILE_SIZE := 8
const EMPTY := Color8(24, 24, 28)

const TERRAIN := {
	"terrain.grassland": Color8(92, 170, 76),
	"terrain.ocean": Color8(35, 83, 158),
	"terrain.desert": Color8(222, 194, 104),
	"terrain.tundra": Color8(188, 202, 202),
	"terrain.swamp": Color8(74, 105, 70),
}

const FEATURE := {
	"feature.forest": Color8(35, 215, 70),
	"feature.mine": Color8(165, 165, 175),
	"feature.oasis": Color8(21, 230, 220),
	"feature.landmark": Color8(255, 238, 45),
	"feature.ruin_village": Color8(232, 96, 223),
	"feature.ruin_town": Color8(203, 55, 195),
	"feature.ruin_city": Color8(150, 28, 158),
}

const RELIEF := {
	"relief.plain": Color8(72, 72, 78),
	"relief.hills": Color8(170, 112, 58),
	"relief.mountain": Color8(235, 235, 242),
}

const EDGE := {
	"stream": Color8(93, 196, 255),
	"river": Color8(31, 132, 255),
	"great_river": Color8(5, 55, 210),
	"trail": Color8(190, 145, 91),
	"road": Color8(145, 91, 49),
	"highway": Color8(87, 48, 24),
	"ancient": Color8(255, 128, 20),
	"crossing": Color8(255, 245, 235),
}

const SETTLEMENT := {
	1: Color8(255, 246, 145),
	2: Color8(255, 170, 35),
	3: Color8(238, 45, 38),
}

const OWNER_NONE := Color8(42, 42, 48)
const OWNER_BORDER := Color8(10, 10, 12)
const OWNER := {
	1: Color8(235, 68, 68),
	2: Color8(70, 126, 245),
	3: Color8(245, 205, 55),
	4: Color8(175, 75, 225),
	5: Color8(45, 205, 150),
	6: Color8(245, 125, 35),
}

# WorldConnectionType 的底層值：海路、山口、地下通道、傳送門。
const PORTAL := {
	0: Color8(40, 235, 255),
	1: Color8(255, 245, 80),
	2: Color8(255, 65, 205),
	3: Color8(245, 245, 255),
}


static func owner(value: int) -> Color:
	if value == 0:
		return OWNER_NONE
	return OWNER.get(value, Color.from_hsv(fmod(float(value) * 0.381966, 1.0), 0.75, 0.95))


static func relief_shade(color: Color, relief_id: String) -> Color:
	if relief_id == "relief.hills":
		return color.lerp(Color8(35, 30, 25), 0.28)
	if relief_id == "relief.mountain":
		return color.lerp(Color8(240, 240, 245), 0.48)
	return color


static func relief(relief_id: String) -> Color:
	return RELIEF.get(relief_id, Color8(255, 0, 255))


static func elevation(value: int, maximum: int) -> Color:
	var ratio := clampf(float(value) / float(maxi(maximum, 1)), 0.0, 1.0)
	if ratio < 0.45:
		return Color8(40, 95, 165).lerp(Color8(88, 180, 75), ratio / 0.45)
	if ratio < 0.78:
		return Color8(88, 180, 75).lerp(Color8(145, 87, 48), (ratio - 0.45) / 0.33)
	return Color8(145, 87, 48).lerp(Color8(250, 250, 250), (ratio - 0.78) / 0.22)


static func temperature(value: int) -> Color:
	var ratio := clampf(float(value) / 255.0, 0.0, 1.0)
	if ratio < 0.5:
		return Color8(30, 80, 255).lerp(Color8(250, 245, 120), ratio * 2.0)
	return Color8(250, 245, 120).lerp(Color8(230, 25, 25), (ratio - 0.5) * 2.0)


static func moisture(value: int) -> Color:
	return Color8(218, 184, 75).lerp(Color8(15, 90, 245), clampf(float(value) / 255.0, 0.0, 1.0))
