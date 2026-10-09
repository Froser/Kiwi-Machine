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

#include <kiwi_main.h>

#if defined(__SWITCH__)
#include <stdio.h>
#include <switch.h>

namespace {
Result g_romfs_result = MAKERESULT(Module_Libnx, LibnxError_InitFail_FS);
Result g_shared_font_result =
    MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
}  // namespace

extern "C" void userAppInit() {
  g_romfs_result = romfsInit();
  g_shared_font_result = plInitialize(PlServiceType_User);
}

extern "C" void userAppExit() {
  if (R_SUCCEEDED(g_shared_font_result))
    plExit();
  if (R_SUCCEEDED(g_romfs_result))
    romfsExit();
}
#endif

#if defined(_WIN32)
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR pCmdLine,
                    int nCmdShow) {
  return KiwiMain(hInstance, hPrevInstance, pCmdLine, nCmdShow);
#elif defined(__SWITCH__)
int main(int argc, char** argv) {
  if (R_FAILED(g_romfs_result)) {
    fprintf(stderr, "Failed to mount embedded RomFS: 0x%08x\n", g_romfs_result);
    return 1;
  }
  if (R_FAILED(g_shared_font_result)) {
    fprintf(stderr, "Failed to initialize shared fonts: 0x%08x\n",
            g_shared_font_result);
    return 1;
  }
  return KiwiMain(argc, argv);
#else
int main(int argc, char** argv) {
  return KiwiMain(argc, argv);
#endif
}
