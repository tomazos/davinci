#pragma once

#include <string>
#include <vector>

namespace dvc {

inline std::vector<std::string> split(const std::string& sep,
                               const std::string joined) {
  std::vector<std::string> result;
  size_t pos = 0;
  while (true) {
    size_t next_pos = joined.find(sep, pos);
    if (next_pos == std::string::npos) {
      result.push_back(joined.substr(pos));
      return result;
    }
    result.push_back(joined.substr(pos, next_pos - pos));
    pos = next_pos + sep.size();
  }
}

}  // namespace dvc
