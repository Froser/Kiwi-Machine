// Copyright (C) 2023 Yisi Yu
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

#include "utility/zip_reader.h"

#include <SDL.h>

#include "third_party/zlib-1.3/zlib.h"

kiwi::nes::Bytes ReadFromZipBinary(const kiwi::nes::Byte* compressed_data,
                                   size_t uncompressed_raw_size,
                                   size_t compressed_data_size) {
  uLongf raw_size_long = static_cast<uLongf>(uncompressed_raw_size);
  kiwi::nes::Bytes bytes;
  bytes.resize(raw_size_long);
  int result = uncompress(bytes.data(), &raw_size_long, compressed_data,
                          static_cast<uLong>(compressed_data_size));
  SDL_assert(result == Z_OK);
  return bytes;
}
