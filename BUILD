cc_binary(
	name = "sdltest",
	srcs = [
			"sdltest.cc",
	],
	linkopts = [
			"-lvulkan",
			"-ldl",
			"-lsndio",
			"-pthread",
			"-lgflags",
			"-lstdc++fs",
	],
	data = [
	    "vert.spv",
	    "frag.spv",
	],
	deps = [
		"//SDL2",
		"//spock",
	],
)

genrule(
    name = "compile_vert_shader",
    srcs = [
        "shader.vert",
    ],
    outs = [
        "vert.spv",
    ],
    cmd = "./$(location glslc) -V -o $@ $(SRCS)",
    tools = [
        "glslc",
    ],
)

genrule(
    name = "compile_frag_shader",
    srcs = [
        "shader.frag",
    ],
    outs = [
        "frag.spv",
    ],
    cmd = "./$(location glslc) -V -o $@ $(SRCS)",
    tools = [
        "glslc",
    ],
)

