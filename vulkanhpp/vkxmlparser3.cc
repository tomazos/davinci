#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>

#include "core/file.h"
#include "vulkanhpp/vulkan_relaxng.h"

DEFINE_string(vkxml, "", "Input vk.xml file");
DEFINE_string(outjson, "", "Output AST to json");

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  CHECK(!FLAGS_vkxml.empty()) << "--vkxml required";

  tinyxml2::XMLDocument doc;
  CHECK(doc.LoadFile(FLAGS_vkxml.c_str()) == tinyxml2::XML_SUCCESS)
      << "Unable to parse " << FLAGS_vkxml;

  auto start = relaxng::parse<vkr::start>(doc.RootElement());

  if (!FLAGS_outjson.empty()) {
    dvc::file_writer fw(FLAGS_outjson, dvc::truncate);
    dvc::json_writer jw(fw.ostream());
    write_json(jw, start);
  }
}
