// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef PACKAGE_MANAGER_UI_H_
#define PACKAGE_MANAGER_UI_H_

struct SDL_Renderer;

void InitializePackageManagerUI(SDL_Renderer* renderer);
void HandlePackageManagerDropFile(const char* dropped_file);
void PaintPackageManagerUI();
void PackageManagerUIPainted();

#endif  // PACKAGE_MANAGER_UI_H_
