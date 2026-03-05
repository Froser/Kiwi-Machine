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

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "utility/math.h"

class MathTest : public testing::Test {
};

TEST_F(MathTest, LerpFloat) {
  // Test float lerp
  EXPECT_FLOAT_EQ(Lerp(0.0f, 100.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(Lerp(0.0f, 100.0f, 0.5f), 50.0f);
  EXPECT_FLOAT_EQ(Lerp(0.0f, 100.0f, 1.0f), 100.0f);
  EXPECT_FLOAT_EQ(Lerp(10.0f, 20.0f, 0.3f), 13.0f);
  
  // Test clamp
  EXPECT_FLOAT_EQ(Lerp(0.0f, 100.0f, -0.5f), 0.0f);
  EXPECT_FLOAT_EQ(Lerp(0.0f, 100.0f, 1.5f), 100.0f);
}

TEST_F(MathTest, LerpRect) {
  // Test rect lerp
  SDL_Rect start = {0, 0, 10, 10};
  SDL_Rect end = {100, 100, 50, 50};
  
  SDL_Rect result = Lerp(start, end, 0.0f);
  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.y, 0);
  EXPECT_EQ(result.w, 10);
  EXPECT_EQ(result.h, 10);
  
  result = Lerp(start, end, 0.5f);
  EXPECT_EQ(result.x, 50);
  EXPECT_EQ(result.y, 50);
  EXPECT_EQ(result.w, 30);
  EXPECT_EQ(result.h, 30);
  
  result = Lerp(start, end, 1.0f);
  EXPECT_EQ(result.x, 100);
  EXPECT_EQ(result.y, 100);
  EXPECT_EQ(result.w, 50);
  EXPECT_EQ(result.h, 50);
}

TEST_F(MathTest, Contains) {
  // Test point in rect
  SDL_Rect rect = {10, 10, 20, 20};
  
  EXPECT_TRUE(Contains(rect, 15, 15));
  EXPECT_TRUE(Contains(rect, 10, 10));
  EXPECT_TRUE(Contains(rect, 29, 29));
  EXPECT_FALSE(Contains(rect, 5, 15));
  EXPECT_FALSE(Contains(rect, 15, 5));
  EXPECT_FALSE(Contains(rect, 30, 15));
  EXPECT_FALSE(Contains(rect, 15, 30));
}

TEST_F(MathTest, Intersect) {
  // Test rect intersection
  SDL_Rect rect1 = {10, 10, 20, 20};
  
  // Intersecting rect
  SDL_Rect rect2 = {15, 15, 20, 20};
  EXPECT_TRUE(Intersect(rect1, rect2));
  
  // Non-intersecting rect
  SDL_Rect rect3 = {35, 35, 10, 10};
  EXPECT_FALSE(Intersect(rect1, rect3));
  
  // Adjacent rect
  SDL_Rect rect4 = {30, 30, 10, 10};
  EXPECT_FALSE(Intersect(rect1, rect4));
  
  // Contained rect
  SDL_Rect rect5 = {15, 15, 5, 5};
  EXPECT_TRUE(Intersect(rect1, rect5));
}

TEST_F(MathTest, Center) {
  // Test center rect
  SDL_Rect parent = {0, 0, 100, 100};
  SDL_Rect child = {0, 0, 20, 20};
  
  SDL_Rect result = Center(parent, child);
  EXPECT_EQ(result.x, 40);
  EXPECT_EQ(result.y, 40);
  EXPECT_EQ(result.w, 20);
  EXPECT_EQ(result.h, 20);
  
  // Test with different sizes
  SDL_Rect child2 = {0, 0, 50, 30};
  result = Center(parent, child2);
  EXPECT_EQ(result.x, 25);
  EXPECT_EQ(result.y, 35);
  EXPECT_EQ(result.w, 50);
  EXPECT_EQ(result.h, 30);
}

TEST_F(MathTest, TriangleBoundingBox) {
  // Test triangle bounding box
  Triangle triangle;
  triangle.point[0] = ImVec2(0, 0);
  triangle.point[1] = ImVec2(10, 0);
  triangle.point[2] = ImVec2(5, 10);
  
  SDL_Rect result = triangle.BoundingBox();
  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.y, 0);
  EXPECT_EQ(result.w, 10);
  EXPECT_EQ(result.h, 10);
  
  // Test with different points
  triangle.point[0] = ImVec2(5, 5);
  triangle.point[1] = ImVec2(15, 15);
  triangle.point[2] = ImVec2(10, 20);
  
  result = triangle.BoundingBox();
  EXPECT_EQ(result.x, 5);
  EXPECT_EQ(result.y, 5);
  EXPECT_EQ(result.w, 10);
  EXPECT_EQ(result.h, 15);
}
