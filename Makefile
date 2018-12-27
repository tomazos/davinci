all:
	find -path ./SDL2 -prune -name '*.h' -or -name '*.cc' | xargs -n 10 clang-format -style=Google -i
	bazel test -c dbg //...
#	bazel run //vulkanhpp:dlvk
#	bazel run //vulkanhpp:vkxmlparser3 -- --vkxml vulkanhpp/vk82.xml --outjson /home/zos/Projects/davinci/vulkanhpp/t.json --outtest /home/zos/Projects/davinci/vulkanhpp/t.cc --logtostderr
#	bazel run //vulkanhpp:relaxng

