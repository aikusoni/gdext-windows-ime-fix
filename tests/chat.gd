extends Control

var field := LineEdit.new()
var closed := false
var presses_after_close := 0

func _ready() -> void:
	var status := GDExtensionManager.load_extension("res://windows_ime_fix.gdextension")
	if status != GDExtensionManager.LOAD_STATUS_OK and status != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		push_error("Could not load windows_ime_fix.gdextension")
		get_tree().quit(1)
		return
	add_child(field)
	field.grab_focus()
	field.edit()
	var ready := FileAccess.open("res://ready.txt", FileAccess.WRITE)
	ready.store_string(str(DisplayServer.window_get_native_handle(DisplayServer.WINDOW_HANDLE)))

func _process(_delta: float) -> void:
	if closed or not FileAccess.file_exists("res://close_chat"):
		return
	field.release_focus()
	field.hide()
	closed = true
	var result := FileAccess.open("res://text.txt", FileAccess.WRITE)
	result.store_line(field.text)

func _input(event: InputEvent) -> void:
	if closed and event is InputEventKey and event.pressed:
		if event.keycode == KEY_F12:
			var result := FileAccess.open("res://presses.txt", FileAccess.WRITE)
			result.store_line(str(presses_after_close))
			get_tree().quit()
		else:
			presses_after_close += 1
