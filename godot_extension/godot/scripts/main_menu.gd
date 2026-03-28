extends Control

@onready var update_text: RichTextLabel = $VBox/UpdateText
@onready var log_text: RichTextLabel = $VBox/LogText
@onready var memory_button: Button = $VBox/Buttons/MemoryTest
@onready var mesher_button: Button = $VBox/Buttons/MesherTest
@onready var ui_button: Button = $VBox/Buttons/UiTest

var extension := BinaryMesherGDExtension.new()

func _ready() -> void:
	memory_button.pressed.connect(_run_memory)
	mesher_button.pressed.connect(_run_mesher)
	ui_button.pressed.connect(_run_ui)

	update_text.text = "[center][b]Update Feed[/b][/center]\n"
	update_text.append_text("• [color=green]System online[/color]\n")
	update_text.append_text("• GDExtension loaded and awaiting debug test input.\n\n")
	update_text.append_text(extension.get_status_message())

	_append_log("Ready. Choose a debug option.")

func _run_memory() -> void:
	_run_test("memory")

func _run_mesher() -> void:
	_run_test("mesher")

func _run_ui() -> void:
	_run_test("ui")

func _run_test(test_name: String) -> void:
	var result: Dictionary = extension.run_debug_test(test_name)
	var status := "PASS" if result.get("passed", false) else "FAIL"
	_append_log("[%s] %s: %s" % [status, result.get("test", test_name), result.get("details", "")])

func _append_log(line: String) -> void:
	log_text.append_text("• %s\n" % line)
