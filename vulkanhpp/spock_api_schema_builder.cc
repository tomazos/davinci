#include "vulkanhpp/spock_api_schema_builder.h"

#include <glog/logging.h>
#include <cctype>
#include <clocale>
#include <set>

#include "core/container.h"

namespace {

enum charkind { underscore, uppercase, lowercase, digit };

charkind to_charkind(char c) {
  if (c == '_') return underscore;
  if (std::isdigit(c)) return digit;
  if (std::islower(c)) return lowercase;
  if (std::isupper(c)) return uppercase;
  LOG(FATAL) << "unexpected char: " << c;
}

std::vector<std::string_view> split_identifier_view(
    std::string_view identifier) {
  std::vector<size_t> poses;
  for (size_t i = 0; i < identifier.size() - 1; i++) {
    charkind a = to_charkind(identifier[i]);
    charkind b = to_charkind(identifier[i + 1]);
    if (a == lowercase && b == uppercase)
      poses.push_back(i + 1);
    else if (a == underscore) {
      poses.push_back(i);
      poses.push_back(i + 1);
    }
  }
  std::vector<std::string_view> splits;
  size_t n = poses.size();
  for (size_t i = 0; i < n + 1; i++) {
    size_t f = (i == 0 ? 0 : poses[i - 1]);
    size_t l = (i == n ? identifier.size() : poses[i]);
    if (l - f == 1 && identifier[f] == '_') continue;
    splits.push_back(identifier.substr(f, l - f));
  }
  return splits;
}

std::string to_underscore_style(const std::vector<std::string_view>& s) {
  std::string o;
  size_t t = s.size() - 1;
  for (auto x : s) t += x.size();
  o.resize(t);
  size_t pos = 0;
  bool first = true;
  for (auto x : s) {
    if (!first) {
      o[pos] = '_';
      pos++;
    } else {
      first = false;
    }

    for (size_t i = 0; i < x.size(); i++) o[pos + i] = std::tolower(x[i]);
    pos += x.size();
  }

  return o;
}

std::string translate_enumeration_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_bitmask_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_enumerator_name(const std::string& name) {
  CHECK(name.substr(0, 3) == "VK_") << name;
  return to_underscore_style(split_identifier_view(name.substr(3)));
}

std::string common_prefix(const std::vector<std::string>& names) {
  CHECK_GT(names.size(), 1);
  for (size_t i = 0; i <= names[0].size(); i++) {
    char c = names[0][i];
    for (size_t j = 1; j < names.size(); j++)
      if (c != names[j][i]) return names[0].substr(0, i);
  }
  LOG(FATAL) << "prefix not proper: " << names[0];
}

// std::string translate_constant_name(const std::string& name) {
//  CHECK(name.substr(0, 3) == "VK_") << name;
//  return name.substr(3);
//}

std::string final_enum_fix(const std::string id) {
  static std::set<std::string> keywords = {"and", "xor", "or", "inline",
                                           "protected"};
  if (keywords.count(id)) return id + "_";
  if (std::isdigit(id[0])) return "n" + id;
  return id;
}

void build_enum(sps::Registry& sreg, const vks::Registry& vreg) {
  std::unordered_set<const vks::Constant*> constants_done;

  auto convert_enumeration = [&](std::string name,
                                 const vks::Enumeration* venumeration) {
    auto senumeration = new sps::Enumeration;
    senumeration->name = translate_enumeration_name(name);
    senumeration->enumeration = venumeration;
    for (const auto& venumerator : venumeration->enumerators) {
      sps::Enumerator senumerator;
      senumerator.name = translate_enumerator_name(venumerator->name);
      senumerator.constant = venumerator;
      constants_done.insert(venumerator);

      senumeration->enumerators.push_back(senumerator);
    }
    std::vector<std::string> unstripped_names;
    unstripped_names.push_back(senumeration->name + "_");
    for (const auto& senumerator : senumeration->enumerators)
      unstripped_names.push_back(senumerator.name);
    size_t strip = common_prefix(unstripped_names).size();
    while (strip != 0) {
      if (unstripped_names.at(0).at(strip - 1) == '_') break;
      strip--;
    }
    for (auto& senumerator : senumeration->enumerators)
      senumerator.name = final_enum_fix(senumerator.name.substr(strip));
    for (const auto& [aname, alias] : vreg.enumerations) {
      if (venumeration == alias && aname != alias->name) {
        senumeration->aliases.push_back(translate_enumeration_name(aname));
      }
    }
    dvc::sort(senumeration->aliases);
    return senumeration;
  };

  std::unordered_set<const vks::Enumeration*> used_enums;

  for (const auto& [name, vbitmask] : vreg.bitmasks) {
    if (name != vbitmask->name) continue;
    sps::Bitmask* bitmask = new sps::Bitmask;
    bitmask->name = translate_bitmask_name(name);
    bitmask->bitmask = vbitmask;
    if (vbitmask->requires) {
      used_enums.insert(vbitmask->requires);
      sps::Enumeration* enumeration =
          convert_enumeration(name, vbitmask->requires);
      for (sps::Enumerator enumerator : enumeration->enumerators) {
        size_t bitpos = enumerator.name.rfind("_bit");
        if (bitpos != std::string::npos)
          enumerator.name = final_enum_fix(enumerator.name.substr(0, bitpos) +
                                           enumerator.name.substr(bitpos + 4));
        bitmask->enumerators.push_back(enumerator);
      }
    }
    for (const auto& [aname, alias] : vreg.bitmasks) {
      if (vbitmask == alias && aname != alias->name) {
        bitmask->aliases.push_back(translate_bitmask_name(aname));
      }
    }
    sreg.bitmasks.push_back(bitmask);
  }
  std::sort(sreg.bitmasks.begin(), sreg.bitmasks.end(),
            [](sps::Bitmask* a, sps::Bitmask* b) { return a->name < b->name; });

  for (const auto& [name, venumeration] : vreg.enumerations) {
    if (used_enums.count(venumeration)) continue;
    if (name != venumeration->name) continue;
    sreg.enumerations.push_back(convert_enumeration(name, venumeration));
  }
  std::sort(sreg.enumerations.begin(), sreg.enumerations.end(),
            [](sps::Enumeration* a, sps::Enumeration* b) {
              return a->name < b->name;
            });

  for (const auto& [name, vconstant] : vreg.constants) {
    if (constants_done.count(vconstant)) continue;
    if (name == "VK_TRUE" || name == "VK_FALSE") continue;
    sps::Constant* sconstant = new sps::Constant;
    sconstant->name = translate_enumerator_name(name);
    sconstant->constant = vconstant;
    sreg.constants.push_back(sconstant);
  }
  std::sort(
      sreg.constants.begin(), sreg.constants.end(),
      [](sps::Constant* a, sps::Constant* b) { return a->name < b->name; });
}

}  // namespace

sps::Registry build_spock_registry(const vks::Registry& vksregistry) {
  sps::Registry spsregistry;

  build_enum(spsregistry, vksregistry);

  return spsregistry;
}
