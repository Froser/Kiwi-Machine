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

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include "../third_party/libjpeg-turbo-jpeg-9f/jpeglib.h"
#include "base/files/file_path.h"

void ShellOpen(const kiwi::base::FilePath& file) {
  @autoreleasepool {
    NSString* ns_file_path =
        [[NSString alloc] initWithUTF8String:file.AsUTF8Unsafe().c_str()];
    NSString* maybe_url_string =
        [ns_file_path stringByAddingPercentEncodingWithAllowedCharacters:
                          [NSCharacterSet URLQueryAllowedCharacterSet]];
    NSURL* maybe_url = [NSURL URLWithString:maybe_url_string];
    if (maybe_url == nil) {
      maybe_url = [NSURL fileURLWithPath:ns_file_path];
    }
    [[NSWorkspace sharedWorkspace] openURL:maybe_url];
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

std::vector<uint8_t> ReadImageAsJPGFromClipboard() {
  @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSArray* types = pasteboard.types;
    std::vector<uint8_t> result;
    NSData* data = nil;
    if ([types containsObject:NSPasteboardTypePNG]) {
      data = [pasteboard dataForType:NSPasteboardTypePNG];
      NSImage* image = [[NSImage alloc] initWithData:data];
      if (image) {
        NSArray* representations = image.representations;
        for (NSBitmapImageRep* bitmap_rep in representations) {
          if ([bitmap_rep isKindOfClass:[NSBitmapImageRep class]]) {
            unsigned char* pixel_data = (unsigned char*)bitmap_rep.bitmapData;
            NSInteger width = bitmap_rep.pixelsWide;
            NSInteger height = bitmap_rep.pixelsHigh;
            return ReadImageAsJPGFromImageData(
                width, height, bitmap_rep.bytesPerRow, pixel_data);
          }
        }
      }
    }
    return result;
  }
}