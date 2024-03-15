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

import "./NESController.css"

export default function NESController() {
  return (
    <div className="nescontroller">
      <div className="nescontroller-base">
        <p className="nescontroller-title">FC/NES 游戏手柄布局</p>
        <div className="nescontroller-front">
          <div className="nescontroller-decoration">
            <div className="nescontroller-stickers">
              <div className="nescontroller-st-a">A</div>
              <div className="nescontroller-st-b">B</div>
              <div className="nescontroller-st-select">SELECT</div>
              <div className="nescontroller-st-start">START</div>
            </div>
            <div className="nescontroller-decoration-central">
              <div></div>
              <div></div>
              <div></div>
              <div></div>
              <div></div>
            </div>
          </div>
          <div className="nescontroller-cross">
            <div className="nescontroller-circle"></div>
            <div className="nescontroller-horizontal">
              <div className="nescontroller-arrowlf"></div>
              <div className="nescontroller-arrowrh"></div>
            </div>
            <div className="nescontroller-vertical">
              <div className="nescontroller-arrowlf"></div>
              <div className="nescontroller-arrowrh"></div>
            </div>
            <div className="nescontroller-back-cross">
              <div className="nescontroller-horiz"></div>
              <div className="nescontroller-vert"></div>
            </div>
          </div>
          <div className="nescontroller-buttons-a-b">
            <div className="nescontroller-btn-border">
              <div className="nescontroller-btn-round nescontroller-a"></div>
            </div>
            <div className="nescontroller-btn-border">
              <div className="nescontroller-btn-round nescontroller-b"></div>
            </div>
          </div>
          <div className="nescontroller-buttons-select">
            <div className="nescontroller-btn-central nescontroller-select"></div>
            <div className="nescontroller-btn-central nescontroller-start"></div>
          </div>
        </div>
      </div>
    </div>
  );
}
