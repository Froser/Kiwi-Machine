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
#include <tuple>
#include <vector>

#include "resources/image_resources.h"

namespace {
std::map<SDL_Renderer*, std::vector<SDL_Texture*>> g_image_resources;
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
      SDL_DestroyTexture(tex_surf_pair);
    }
  }

  IMG_Quit();
}

SDL_Texture* GetImage(SDL_Renderer* renderer, image_resources::ImageID id) {
  bool is_renderer_registered =
      g_image_resources.find(renderer) != g_image_resources.end();
  if (!is_renderer_registered)
    g_image_resources[renderer].resize(
        static_cast<int>(image_resources::ImageID::kLast));

  // If texture hasn't been created for |renderer| and |id|:
  if (!g_image_resources[renderer][static_cast<int>(id)]) {
    size_t size;
    const unsigned char* data = image_resources::GetData(id, &size);
    SDL_RWops* bg_res = SDL_RWFromMem(const_cast<unsigned char*>(data), size);
    SDL_Texture* texture =
        IMG_LoadTextureTyped_RW(renderer, bg_res, 1, nullptr);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
    g_image_resources[renderer][static_cast<int>(id)] = texture;
  }

  SDL_assert(g_image_resources[renderer][static_cast<int>(id)]);
  return g_image_resources[renderer][static_cast<int>(id)];
}
