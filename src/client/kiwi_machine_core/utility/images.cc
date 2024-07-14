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

#include "utility/images.h"

#include <SDL.h>
#include <SDL_image.h>
#include <map>
#include <vector>

#include "resources/image_resources.h"

namespace {
std::map<SDL_Renderer*, std::map<image_resources::ImageID, SDL_Texture*>>
    g_image_resources;
std::map<image_resources::ImageID, kiwi::nes::Bytes> g_extern_image_data_map;
static size_t g_extern_map_data_id =
    static_cast<size_t>(image_resources::ImageID::kLast) + 1;
}  // namespace

bool InitializeImageResources() {
  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags))
    return false;

  return true;
}

void UninitializeImageResources() {
  for (const auto& res : g_image_resources) {
    for (const auto& tex_surf_pair : res.second) {
      SDL_DestroyTexture(tex_surf_pair.second);
    }
  }

  IMG_Quit();
}

SDL_Texture* GetImage(SDL_Renderer* renderer, image_resources::ImageID id) {
  // If texture hasn't been created for |renderer| and |id|:
  if (!g_image_resources[renderer][id]) {
    size_t size = 0;
    const unsigned char* data = nullptr;
    if (id < image_resources::ImageID::kLast) {
      data = image_resources::GetData(id, &size);
    } else {
      const kiwi::nes::Bytes& image_data = g_extern_image_data_map[id];
      data = image_data.data();
      size = image_data.size();
    }
    SDL_assert(size > 0 && data);
    SDL_RWops* bg_res = SDL_RWFromMem(const_cast<unsigned char*>(data), size);
    SDL_Texture* texture =
        IMG_LoadTextureTyped_RW(renderer, bg_res, 1, nullptr);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
    g_image_resources[renderer][id] = texture;
  }

  SDL_assert(g_image_resources[renderer][id]);
  return g_image_resources[renderer][id];
}

image_resources::ImageID ImageRegister(const kiwi::nes::Bytes& data) {
  image_resources::ImageID id =
      static_cast<image_resources::ImageID>(g_extern_map_data_id++);
  g_extern_image_data_map[id] = data;
  return id;
}

void ImageUnregister(image_resources::ImageID image_id) {
  kiwi::nes::Bytes dummy;
  g_extern_image_data_map[image_id].swap(dummy);
  g_extern_image_data_map.erase(image_id);
}
