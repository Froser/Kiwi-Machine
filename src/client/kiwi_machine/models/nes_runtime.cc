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

#include "models/nes_runtime.h"

#include <vector>

namespace {
std::vector<std::unique_ptr<NESRuntime::Data>> g_runtime_data;
}

NESRuntime::NESRuntime() = default;
NESRuntime::~NESRuntime() = default;

NESRuntime::Data* NESRuntime::GetDataById(NESRuntimeID id) {
  return g_runtime_data[id].get();
}

NESRuntimeID NESRuntime::CreateData() {
  g_runtime_data.push_back(std::make_unique<NESRuntime::Data>());
  return g_runtime_data.size() - 1;
}

NESRuntime* NESRuntime::GetInstance() {
  static NESRuntime g_instance;
  return &g_instance;
}