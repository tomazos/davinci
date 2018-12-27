#include <tinyxml2.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tinyxml2;

#define DUMP(expr)                                                 \
  do {                                                             \
    std::cout << "DUMP " << #expr << " = " << (expr) << std::endl; \
  } while (0)

#define ASSERT(cond)                          \
  do {                                        \
    if (!(cond)) {                            \
      std::cerr << "!" << #cond << std::endl; \
      std::exit(EXIT_FAILURE);                \
    }                                         \
  } while (0)

template <class... Args>
[[noreturn]] inline void Error(Args&&... args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  throw std::runtime_error(oss.str());
};

class ElementHandler {
 public:
  virtual ~ElementHandler() = default;
  virtual std::unique_ptr<ElementHandler> enter(const ElementInfo& e) = 0;
  virtual void ontext(const std::string& text){};
};

class NullHandler : public ElementHandler {
 public:
  std::unique_ptr<ElementHandler> enter(const ElementInfo& e) override {
    return std::make_unique<NullHandler>();
  }
};

std::vector<std::unique_ptr<ElementHandler>> handlers;

class RegistryHandler : public ElementHandler {
 public:
  std::unique_ptr<ElementHandler> enter(const ElementInfo& e) override {
    auto n = e.name;
    if (n == "enums") {
      process_enums(registry_entry);
    } else if (n == "comment") {
      /*pass*/
    } else if (n == "platforms") {
      process_platforms(registry_entry);
    } else if (n == "tags") {
      process_tags(registry_entry);
    } else if (n == "types") {
      process_types(registry_entry);
    } else if (n == "commands") {
      process_commands(registry_entry);
    } else if (n == "feature") {
      process_feature(registry_entry);
    } else if (n == "extensions") {
      process_extensions(registry_entry);
    } else {
      Error("unknown registry_entry ", n);
    }

    return std::make_unique<NullHandler>();
  }
};

class VkXMLVisitor : public XMLVisitor {
 public:
  bool VisitEnter(const XMLElement& xml_element, const XMLAttribute*) override {
    ElementInfo e(&xml_element);
    if (handlers.empty()) {
      ASSERT(e.name == "registry");
      handlers.push_back(std::make_unique<RegistryHandler>());
      return true;
    } else {
      handlers.push_back(handlers.back()->enter(e));
      return true;
    }
  }

  bool Visit(const XMLText& text) override {
    handlers.back()->ontext(text.Value());
    return true;
  }

  bool VisitExit(const XMLElement& /*element*/) override {
    ASSERT(!handlers.empty());
    handlers.pop_back();
    return true;
  }

  bool Visit(const XMLComment& /*comment*/) override { return true; }

  bool Visit(const XMLUnknown& /*unknown*/) override { Error("XMLUnknown"); }
};

int main(int argc, char** argv) {
  try {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string vkxml_path = args.at(0);
    XMLDocument doc;

    XMLError error = doc.LoadFile(vkxml_path.c_str());
    if (error != XML_SUCCESS) Error("Unable to parse ", vkxml_path);

    VkXMLVisitor visitor;
    doc.Accept(&visitor);

  } catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
