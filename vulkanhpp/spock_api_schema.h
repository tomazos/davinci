#pragma once

#include "vulkanhpp/vulkan_api_schema.h"

namespace sps {

struct Entity {
  std::string name;
};

struct Enumerator : Entity {
  const vks::Constant* constant;
};

struct Enumeration : Entity {
  const vks::Enumeration* enumeration;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Bitmask : Entity {
  const vks::Bitmask* bitmask;
  std::vector<Enumerator> enumerators;
  std::vector<std::string> aliases;
};

struct Constant : Entity {
  const vks::Constant* constant;
};

struct Registry {
  std::vector<Enumeration*> enumerations;
  std::vector<Bitmask*> bitmasks;
  std::vector<Constant*> constants;
};

}  // namespace sps
