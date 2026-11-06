extends Control
## One input field and a log of what Godot itself saw, to lay beside what the probe saw in front of it. Loads the probe only
## where it can run, the way a real use would have to: no DLL or another OS leaves a plain Godot field and no error.
const EXTENSION := "res://ime_probe.gdextension"
const LIBRARY := "res://bin/ime_probe.dll"

var _field: LineEdit
var _shown: Label
var _log: FileAccess
var _last_ime := ""

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var font := SystemFont.new()
	font.font_names = PackedStringArray(["Malgun Gothic", "Yu Gothic", "Segoe UI"])
	var look := Theme.new()
	look.default_font = font
	look.default_font_size = 28
	theme = look
	var column := VBoxContainer.new()
	column.set_anchors_preset(Control.PRESET_FULL_RECT)
	column.add_theme_constant_override("separation", 12)
	add_child(column)
	_shown = Label.new()
	_shown.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_field = LineEdit.new()
	_field.custom_minimum_size.y = 64
	# Enter would otherwise end editing and turn the input method off, and the next word would arrive as plain keys.
	_field.keep_editing_on_text_submit = true
	column.add_child(_field)
	column.add_child(_shown)
	var path := OS.get_environment("IME_PROBE_GODOT_LOG")
	_log = FileAccess.open(path if not path.is_empty() else "user://ime_probe_godot.log", FileAccess.WRITE)
	var loaded := "not loaded"
	if OS.has_feature("windows") and FileAccess.file_exists(LIBRARY) and not "--no-probe" in args:
		loaded = str(GDExtensionManager.load_extension(EXTENSION))
	_write("START display=%s probe=%s" % [DisplayServer.get_name(), loaded])
	_field.text_changed.connect(func(value: String) -> void: _write("text_changed [%s]" % value))
	_field.text_submitted.connect(func(value: String) -> void: _write("text_submitted [%s]" % value))
	_field.grab_focus()
	_refresh()
	if "--autotest" in args:
		await get_tree().create_timer(2.5).timeout
		_write("AUTOTEST text=[%s] ime_active_focus=%s" % [_field.text, _field.has_focus()])
		print("IME_PROBE_AUTOTEST text=[%s] probe=%s" % [_field.text, loaded])
		_log.flush()
		get_tree().quit()

func _notification(what: int) -> void:
	if what != MainLoop.NOTIFICATION_OS_IME_UPDATE or _field == null: return
	var ime := DisplayServer.ime_get_text()
	_write("ime_update text=[%s] selection=%s field=[%s]" % [ime, DisplayServer.ime_get_selection(), _field.text])
	_last_ime = ime
	_refresh()

func _write(line: String) -> void:
	if _log != null:
		_log.store_line("%.3f %s" % [Time.get_unix_time_from_system(), line])
		_log.flush()
	_refresh()

func _refresh() -> void:
	if _shown != null and _field != null: _shown.text = "조합 중(Godot): [%s]\n입력칸: [%s]" % [_last_ime, _field.text]
