extends SceneTree

# Aggregator that runs all GDScript test suites and reports a single exit code.

const SUITES := [
	"res://demo/tests/test_cpu.gd",
	"res://demo/tests/test_gpu.gd",
]

func _init() -> void:
	var failed := 0
	var godot_exe := OS.get_executable_path()

	for path in SUITES:
		print("\n=== Running ", path, " ===")
		var output: Array[String] = []
		var code := OS.execute(godot_exe, ["--headless", "--script", path], output, true)
		for line in output:
			print(line)
		if code != 0:
			failed += 1
			push_error("FAILED: " + path + " (exit code " + str(code) + ")")
		else:
			print("PASSED: ", path)

	print("\n=== Summary ===")
	print("%d/%d suites failed" % [failed, SUITES.size()])
	quit(0 if failed == 0 else 1)
