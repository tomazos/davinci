cc_library(
  name = "libvulkan",
  hdrs = [
    "LibVulkan.h",
  ],
  srcs = [
    "LibVulkan.cc",
  ],
)

cc_binary(
  name = "vulkan_hpp_generator",
  srcs = [
      "VulkanHppGenerator.cpp",
  ],
  linkopts = [
      "-ltinyxml2",
  ],
)

cc_binary(
  name = "vkxmlparser",
  srcs = [
      "vkxmlparser.cc",
  ],
  data = [
      "vk.xml",
      "vk82.xml",
  ],
  linkopts = [
        "-ltinyxml2",
  ],
)

#cc_binary(
#  name = "vkxmlparser2",
#  srcs = [
#      "vkxmlparser2.cc",
#  ],
#  linkopts = [
#      "-ltinyxml2",
#  ],
#)

cc_binary(
  name = "vkxmlparser3",
  srcs = [
      "vkxmlparser3.cc",
  ],
    data = [
      "vk.xml",
      "vk82.xml",
  ],
  linkopts = [
      "-ltinyxml2",
      "-lgflags",
      "-lglog",
      "-lstdc++fs",
  ],
  deps = [
      ":vulkan_relaxng",
      "//core:json",
      "//core:file",
  ],
)

cc_binary(
  name = "dlvk",
  srcs = [
    "dlvk.cc",
  ],
  linkopts = [
    "-ldl",
  ],
  deps = [
    ":libvulkan",
  ],
)


genrule(
  name = "vulkan_relaxng_generate",
  srcs = [
     "registry.rnc",
  ],
  outs = [
     "vulkan_relaxng.h",
  ],
  cmd = "$(location //relaxng:relaxngc) --namespace vkr --protocol Vulkan82 --schema $(location registry.rnc) --hout $(location vulkan_relaxng.h)",
  tools = [
     "//relaxng:relaxngc",
  ],
)

cc_library(
  name = "vulkan_relaxng",
  hdrs = [
    "vulkan_relaxng.h",
  ],
  deps = [
    "//relaxng",
  ],
)

cc_test(
  name = "vulkan_relaxng_test",
  srcs = [
     "vulkan_relaxng_test.cc",
  ],
  deps = [
     ":vulkan_relaxng",
  ],
)