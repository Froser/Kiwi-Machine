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

import "./Footer.css"

export default function Footer() {
  return (
    <footer className="footer">
      <span>已备案</span>
      <a href="http://beian.miit.gov.cn" target="_blank" rel="noreferrer">粤ICP备2024200615号-1</a>
      <span>纯个人交流，禁止用于商业目的</span>
      <span>By 于益偲</span>
    </footer>
  );
}
