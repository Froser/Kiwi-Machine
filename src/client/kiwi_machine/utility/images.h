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

#ifndef UTILITY_IMAGES_H_
#define UTILITY_IMAGES_H_

#include "resources/image_resources.h"

struct SDL_Texture;
struct SDL_Renderer;
bool InitializeImageResources();
void UninitializeImageResources();
SDL_Texture* GetImage(SDL_Renderer* renderer, image_resources::ImageID id);

#endif  // UTILITY_IMAGES_H_