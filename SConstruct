#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

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
