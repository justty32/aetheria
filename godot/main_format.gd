extends RefCounted


const EVENT_NAMES := [
	"新遊戲已建立", "玩家下達移動意圖", "一旬結算完成", "勢力 AI 已行動",
	"敵軍移動", "發生敵軍遭遇", "戰鬥已結算", "戰後世界已改變",
	"進入 Site", "返回 Region", "城建完成", "進入 Local", "開門並移動",
	"接受湧現任務", "完成湧現任務", "進入負 z 地城", "地城清空",
	"返回 Site", "締結條約", "交給系統處理",
]
const AI_ACTIONS := ["發展", "備戰", "擴張", "宣戰", "提議同盟", "進貢", "統計推進"]
const OUTCOMES := ["戰鬥持續", "我軍潰散", "敵軍潰散", "雙方脫離"]
const FATE_OUTCOMES := ["安然無恙", "負傷", "財產損失", "流離失所", "陣亡"]


static func _format_coverage(value: Dictionary) -> String:
	var hashes: PackedStringArray = value["roundtrip_hashes"]
	var hash_text := "尚未量測" if hashes.is_empty() else " / ".join(hashes)
	var sign := "+" if value["signed_relative_error_percent"] >= 0.0 else ""
	return "霧橋鎮 Region 真值：建設=%d（最近 %d→%d）／治安=%d（最近 %d→%d）／產出=%d\n城建完成=%d；任務=%d（清剿 可用=%s、已接=%s；地城 可用=%s）\n地城：cleared=%s；敵人密度 %d→%d；和平條約=%d\nSite 往返 hash：%s\n親自 vs 系統 N=%d：%d vs %d；相對誤差 %s%.3f%%" % [
		value["development"], value["last_development_before"], value["last_development_after"],
		value["order"], value["last_order_before"], value["last_order_after"], value["production"],
		value["city_buildings"], value["quest_count"], value["bandit_quest_available"],
		value["bandit_quest_accepted"], value["dungeon_quest_available"], value["dungeon_cleared"],
		value["dungeon_density_before"], value["dungeon_density_after"], value["treaty_count"], hash_text,
		value["calibration_n"], value["manual_total"], value["managed_total"], sign,
		value["signed_relative_error_percent"],
	]


static func _format_battle(report: Dictionary) -> String:
	if report.is_empty():
		return "尚未交戰"
	var before: Dictionary = report["before"]
	var after: Dictionary = report["after"]
	return "%s\n我軍傷亡 %d；敵軍 %d；%s\n%s — %s\n世界：owner %d→%d；人口 %d→%d；治安 %d→%d" % [
		"親自指揮 Site" if report["choice"] == "manual" else "系統計算 Region",
		report["loss_a"], report["loss_b"], OUTCOMES[report["outcome"]], report["named_person"],
		FATE_OUTCOMES[report["named_outcome"]], before["owner"], after["owner"],
		before["population"], after["population"], before["order"], after["order"],
	]


static func _format_events(events: Array) -> String:
	var lines := PackedStringArray()
	for index in range(maxi(0, events.size() - 9), events.size()):
		var event: Dictionary = events[index]
		var kind: int = event["kind"]
		var description: String = EVENT_NAMES[kind] if kind < EVENT_NAMES.size() else "事件"
		if kind == 3:
			description += "：勢力 %d／%s" % [event["a"], AI_ACTIONS[event["b"]]]
		elif kind == 5:
			description += "：戰力 %d vs %d" % [event["a"], event["b"]]
		lines.append("#%d %s @(%d,%d)　數字=%d→%d" % [event["id"], description, event["x"], event["y"], event["a"], event["b"]])
	return "\n".join(lines)


static func _visible_dump(node: Node) -> String:
	var lines := PackedStringArray()
	_collect_visible_text(node, lines)
	return "\n".join(lines)


static func _collect_visible_text(node: Node, lines: PackedStringArray) -> void:
	if node is CanvasItem and not node.visible:
		return
	if node is Label:
		lines.append(node.text)
	elif node is Button:
		lines.append("[%s]" % node.text)
	for child in node.get_children():
		_collect_visible_text(child, lines)
