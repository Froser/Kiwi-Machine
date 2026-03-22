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

import "./LandscapeTips.css"
import {Dispatch, SetStateAction, useEffect, useState} from "react";
import {isMobileDevice} from "../../services/device";

interface LandscapeTipsProps {
  show: boolean,
  setVisible: Dispatch<SetStateAction<boolean>>
}

export default function LandscapeTips({show, setVisible}: LandscapeTipsProps) {
  const [visible, setLocalVisible] = useState(false);

  useEffect(() => {
    if (!isMobileDevice()) return;

    if (show) {
      setLocalVisible(true);
      const timer = setTimeout(() => {
        setLocalVisible(false);
        setVisible(false);
      }, 2000);
      return () => clearTimeout(timer);
    }
  }, [show, setVisible]);

  return (
    <div className={`landscape-tips ${visible ? 'landscape-tips-show' : ''}`}>
      <div className="landscape-tips-content">
        <span className="landscape-tips-text">为了更好的体验，建议关闭浏览器的工具栏进行游戏。</span>
      </div>
    </div>
  );
}
