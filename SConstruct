#!/usr/bin/env python
import os
import sys

# Allow developers to keep the heavy godot-cpp submodule outside the Godot
# project tree (e.g. ../godot-cpp) so the editor does not scan thousands of
# files on every open. Falls back to the bundled godot-cpp/ directory.
godot_cpp_path = os.environ.get("GODOT_CPP_PATH", "godot-cpp")
sconstruct_path = os.path.join(godot_cpp_path, "SConstruct")
if not os.path.isfile(sconstruct_path):
    print("Error: godot-cpp SConstruct not found at {}".format(sconstruct_path))
    print("Set GODOT_CPP_PATH to a valid godot-cpp checkout, or keep it at ./godot-cpp.")
    sys.exit(1)

# If the heavy godot-cpp tree lives inside the Godot project, hide it from the
# editor file-system scan so opening the project does not crawl thousands of
# files. This is a no-op when GODOT_CPP_PATH points outside the project.
if not os.path.isabs(godot_cpp_path) and not godot_cpp_path.startswith(".."):
    gdignore = os.path.join(godot_cpp_path, ".gdignore")
    if not os.path.exists(gdignore):
        with open(gdignore, "w"):
            pass

# Hide distribution/temp directories from the editor scan as well.
for hide_dir in ["dist", "tmp_source"]:
    if not os.path.isdir(hide_dir):
        os.makedirs(hide_dir, exist_ok=True)
    gdignore = os.path.join(hide_dir, ".gdignore")
    if not os.path.exists(gdignore):
        with open(gdignore, "w"):
            pass

env = SConscript(sconstruct_path)

# Add source files
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")
sources += Glob("src/api/*.cpp")
sources += Glob("src/core/*.cpp")
sources += Glob("src/cpu/*.cpp")
sources += Glob("src/gpu/*.cpp")

# Library output path
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "addons/cellular_automata_engine/bin/libcellauto.{}.{}.framework/libcellauto.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "addons/cellular_automata_engine/bin/libcellauto{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
