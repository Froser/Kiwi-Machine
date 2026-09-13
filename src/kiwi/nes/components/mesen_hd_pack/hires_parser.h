// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifndef NES_COMPONENTS_MESEN_HD_PACK_HIRES_PARSER_H_
#define NES_COMPONENTS_MESEN_HD_PACK_HIRES_PARSER_H_

#include <string_view>
#include <vector>

#include "nes/components/mesen_hd_pack/hd_pack_types.h"

namespace kiwi {
namespace nes {
namespace mesen_hd_pack {

class HiresParser {
 public:
  HiresParser() = default;
  ~HiresParser() = default;

  HiresParser(const HiresParser&) = delete;
  HiresParser& operator=(const HiresParser&) = delete;

  // Parses a Mesen 2 hires.txt file. |data| and |errors| are cleared before
  // parsing. A false return value means at least one invalid rule was found.
  bool Parse(std::string_view contents,
             HdPackData* data,
             std::vector<ParseError>* errors) const;
};

}  // namespace mesen_hd_pack
}  // namespace nes
}  // namespace kiwi

#endif  // NES_COMPONENTS_MESEN_HD_PACK_HIRES_PARSER_H_
