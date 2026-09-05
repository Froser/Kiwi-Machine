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

#include "utility/algorithm.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

class AlgorithmTest : public testing::Test {
};

TEST_F(AlgorithmTest, HasStringBasic) {
  // Test basic functionality
  EXPECT_TRUE(HasString("hello", "h"));
  EXPECT_TRUE(HasString("hello", "he"));
  EXPECT_TRUE(HasString("hello", "hel"));
  EXPECT_TRUE(HasString("hello", "hell"));
  EXPECT_TRUE(HasString("hello", "hello"));
  EXPECT_FALSE(HasString("hello", "world"));
  EXPECT_FALSE(HasString("hello", "helloo"));
}

TEST_F(AlgorithmTest, HasStringCaseInsensitive) {
  // Test case insensitivity
  EXPECT_TRUE(HasString("Hello", "h"));
  EXPECT_TRUE(HasString("HELLO", "he"));
  EXPECT_TRUE(HasString("hello", "HEL"));
  EXPECT_FALSE(HasString("Hello", "WORLD"));
  EXPECT_TRUE(HasString("HELLO WORLD", "world"));
}

TEST_F(AlgorithmTest, HasStringEmpty) {
  // Test empty strings
  EXPECT_TRUE(HasString("", ""));
  EXPECT_FALSE(HasString("", "a"));
  EXPECT_TRUE(HasString("hello", ""));
}

TEST_F(AlgorithmTest, HasStringOrder) {
  // Test character order
  EXPECT_TRUE(HasString("abcde", "ace"));
  EXPECT_TRUE(HasString("abcde", "ade"));
  EXPECT_FALSE(HasString("abcde", "aec"));
  EXPECT_FALSE(HasString("abcde", "eda"));
}

TEST_F(AlgorithmTest, HasStringSingleCharacter) {
  // Test single character
  EXPECT_TRUE(HasString("a", "a"));
  EXPECT_FALSE(HasString("a", "b"));
  EXPECT_TRUE(HasString("A", "a"));
  EXPECT_TRUE(HasString("a", "A"));
}

TEST_F(AlgorithmTest, HasStringUtf8Chinese) {
  // The needle (2nd arg) must be an ordered subsequence of the haystack (1st
  // arg), matched by whole UTF-8 code points.
  EXPECT_TRUE(HasString("超级马里奥兄弟", "马里奥"));
  EXPECT_TRUE(HasString("超级马里奥兄弟", "马"));
  EXPECT_TRUE(HasString("超级马里奥兄弟", "超奥弟"));  // subsequence
  EXPECT_FALSE(HasString("超级马里奥兄弟", "奥马"));    // wrong order
  EXPECT_FALSE(HasString("超级马里奥兄弟", "坦克"));
  EXPECT_TRUE(HasString("魂斗罗", ""));  // empty needle matches
}

TEST_F(AlgorithmTest, HasStringUtf8Mixed) {
  // Mixed ASCII + CJK, ASCII stays case-insensitive.
  EXPECT_TRUE(HasString("Final Fantasy 最终幻想", "最终"));
  EXPECT_TRUE(HasString("Final Fantasy 最终幻想", "ff"));  // subsequence of ascii
  EXPECT_FALSE(HasString("最终幻想", "fantasy"));
}
