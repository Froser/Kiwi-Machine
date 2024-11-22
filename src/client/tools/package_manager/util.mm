// Copyright (C) 2024 Yisi Yu
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

#include "util.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"

kiwi::base::FilePath GetDefaultSavePath() {
  @autoreleasepool {
    NSString* documents_dir =
        [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
    const char* u8_documents_dir =
        [documents_dir cStringUsingEncoding:NSUTF8StringEncoding];
    return kiwi::base::FilePath::FromUTF8Unsafe(u8_documents_dir);
  }
}

void ShellOpen(const kiwi::base::FilePath& file) {
  @autoreleasepool {
    NSString* ns_file_path =
        [[NSString alloc] initWithUTF8String:file.AsUTF8Unsafe().c_str()];
    NSURL* url = [NSURL fileURLWithPath:ns_file_path];
    [[NSWorkspace sharedWorkspace] openURL:url];
  }
}

void ShellOpenDirectory(const kiwi::base::FilePath& file) {
  @autoreleasepool {
    NSString* ns_file_path =
        [[NSString alloc] initWithUTF8String:file.AsUTF8Unsafe().c_str()];
    NSURL* url = [NSURL fileURLWithPath:ns_file_path];
    url = [url URLByDeletingLastPathComponent];
    [[NSWorkspace sharedWorkspace] openURL:url];
  }
}

void RunExecutable(const kiwi::base::FilePath& bundle,
                   const std::vector<std::string>& args) {
  @autoreleasepool {
    kiwi::base::FilePath app_path =
        bundle.Append(FILE_PATH_LITERAL("Contents"))
            .Append(FILE_PATH_LITERAL("MacOS"))
            .Append(bundle.BaseName().RemoveExtension());
    NSString* bundle_path =
        [[NSString alloc] initWithUTF8String:app_path.AsUTF8Unsafe().c_str()];
    NSTask* task = [[NSTask alloc] init];
    task.launchPath = bundle_path;
    NSMutableArray* arguments = [[NSMutableArray alloc] init];
    for (const auto& arg : args) {
      [arguments addObject:[[NSString alloc] initWithUTF8String:arg.c_str()]];
    };
    task.arguments = arguments;
    [task launch];
  }
}