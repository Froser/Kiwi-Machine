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
std::map<SDL_Renderer*, std::vector<std::pair<SDL_Texture*, SDL_Surface*>>>
    g_image_resources;

SDL_Texture* CreateLogoTexture(SDL_Renderer* renderer,
                               image_resources::ImageID id,
                               const unsigned char* data,
                               size_t size) {
  SDL_RWops* bg_res = SDL_RWFromMem(const_cast<unsigned char*>(data), size);
  SDL_Surface* surface = IMG_Load_RW(bg_res, 1);
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
  return texture;
}

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
      SDL_DestroyTexture(tex_surf_pair.first);
      SDL_FreeSurface(tex_surf_pair.second);
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
  if (!g_image_resources[renderer][static_cast<int>(id)].first) {
    size_t size;
    const unsigned char* data = image_resources::GetData(id, &size);
    SDL_RWops* bg_res = SDL_RWFromMem(const_cast<unsigned char*>(data), size);
    SDL_Surface* surface = IMG_Load_RW(bg_res, 1);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);
    g_image_resources[renderer][static_cast<int>(id)] =
        std::pair(texture, surface);
  }

  SDL_assert(g_image_resources[renderer][static_cast<int>(id)].first &&
             g_image_resources[renderer][static_cast<int>(id)].second);
  return g_image_resources[renderer][static_cast<int>(id)].first;
}
