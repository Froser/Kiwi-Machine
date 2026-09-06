// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "ui/window_base.h"

#include <jni.h>
#include <atomic>

namespace {
std::atomic<int> g_safe_inset_left{0};
std::atomic<int> g_safe_inset_top{0};
std::atomic<int> g_safe_inset_right{0};
std::atomic<int> g_safe_inset_bottom{0};
}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_yuyisi_kiwimachine_MainActivity_nativeSetKiwiSafeAreaInsets(
    JNIEnv*,
    jclass,
    jint left,
    jint top,
    jint right,
    jint bottom) {
  g_safe_inset_left.store(left, std::memory_order_relaxed);
  g_safe_inset_top.store(top, std::memory_order_relaxed);
  g_safe_inset_right.store(right, std::memory_order_relaxed);
  g_safe_inset_bottom.store(bottom, std::memory_order_relaxed);
}

SDL_Rect WindowBase::GetSafeAreaInsets() {
  return SDL_Rect{
      g_safe_inset_left.load(std::memory_order_relaxed),
      g_safe_inset_top.load(std::memory_order_relaxed),
      g_safe_inset_right.load(std::memory_order_relaxed),
      g_safe_inset_bottom.load(std::memory_order_relaxed),
  };
}
