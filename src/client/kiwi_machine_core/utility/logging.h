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

#ifndef UTILITY_LOGGING_H_
#define UTILITY_LOGGING_H_

#include "build/kiwi_defines.h"

// WASM_TRACE_LOG is a conditional compilation logging macro for WebAssembly
// platform debugging. It provides different implementations based on whether
// KIWI_WASM is defined.

#if KIWI_WASM

// Logging implementation enabled on WebAssembly platform
#include <iostream>

// TraceLogHelper is a helper class for chained logging output
// It automatically adds a newline in its destructor to ensure each log
// output is a complete line
class TraceLogHelper {
 public:
  // Overload << operator to support chained output of any type
  template <typename Any>
  TraceLogHelper& operator<<(const Any& any) {
    std::cout << any;
    return *this;
  }
  // Destructor automatically outputs a newline to complete a log record
  ~TraceLogHelper() { std::cout << std::endl; }
};

// WASM_TRACE_LOG macro definition: Output logs on WebAssembly platform
// Usage example: WASM_TRACE_LOG << "Variable value: " << x;
// It automatically prepends the current function name (__func__ macro)
#define WASM_TRACE_LOG TraceLogHelper()

#else

// Empty implementation on non-WebAssembly platforms, no logging output
// This allows zero-overhead removal of debug logs on other platforms

// DoNothingTraceLog is a no-op class, all output operations are ignored
class DoNothingTraceLog {
 public:
  // Overload << operator but do nothing
  template <typename Any>
  DoNothingTraceLog& operator<<(const Any&) {
    return *this;
  }
};

// WASM_TRACE_LOG macro definition: No log output on non-WebAssembly platforms
// This ensures no logging overhead when compiling on other platforms
#define WASM_TRACE_LOG DoNothingTraceLog()

#endif

#endif  // UTILITY_LOGGING_H_