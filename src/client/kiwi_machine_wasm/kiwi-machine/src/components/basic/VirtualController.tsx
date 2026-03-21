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

import "./VirtualController.css"
import { useState, useCallback } from "react";

export type ControllerButton = 'up' | 'down' | 'left' | 'right' | 'a' | 'b' | 'select' | 'start';

interface VirtualControllerProps {
  onButtonPress?: (button: ControllerButton) => void;
  onButtonRelease?: (button: ControllerButton) => void;
}

export default function VirtualController({ onButtonPress, onButtonRelease }: VirtualControllerProps) {
  const [activeButtons, setActiveButtons] = useState<Set<ControllerButton>>(new Set());

  const handleButtonDown = useCallback((button: ControllerButton, e: React.MouseEvent | React.TouchEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setActiveButtons(prev => new Set(prev).add(button));
    onButtonPress?.(button);
  }, [onButtonPress]);

  const handleButtonUp = useCallback((button: ControllerButton, e: React.MouseEvent | React.TouchEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setActiveButtons(prev => {
      const next = new Set(prev);
      next.delete(button);
      return next;
    });
    onButtonRelease?.(button);
  }, [onButtonRelease]);

  const isActive = (button: ControllerButton) => activeButtons.has(button);

  return (
    <div className="virtual-controller">
      <div className="virtual-controller-wrapper">
        <div className="virtual-dpad">
          <div className="virtual-dpad-center"></div>
          <button
            className={`virtual-dpad-btn virtual-dpad-up ${isActive('up') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('up', e)}
            onMouseUp={(e) => handleButtonUp('up', e)}
            onMouseLeave={(e) => handleButtonUp('up', e)}
            onTouchStart={(e) => handleButtonDown('up', e)}
            onTouchEnd={(e) => handleButtonUp('up', e)}
          >
            ▲
          </button>
          <button
            className={`virtual-dpad-btn virtual-dpad-down ${isActive('down') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('down', e)}
            onMouseUp={(e) => handleButtonUp('down', e)}
            onMouseLeave={(e) => handleButtonUp('down', e)}
            onTouchStart={(e) => handleButtonDown('down', e)}
            onTouchEnd={(e) => handleButtonUp('down', e)}
          >
            ▼
          </button>
          <button
            className={`virtual-dpad-btn virtual-dpad-left ${isActive('left') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('left', e)}
            onMouseUp={(e) => handleButtonUp('left', e)}
            onMouseLeave={(e) => handleButtonUp('left', e)}
            onTouchStart={(e) => handleButtonDown('left', e)}
            onTouchEnd={(e) => handleButtonUp('left', e)}
          >
            ◀
          </button>
          <button
            className={`virtual-dpad-btn virtual-dpad-right ${isActive('right') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('right', e)}
            onMouseUp={(e) => handleButtonUp('right', e)}
            onMouseLeave={(e) => handleButtonUp('right', e)}
            onTouchStart={(e) => handleButtonDown('right', e)}
            onTouchEnd={(e) => handleButtonUp('right', e)}
          >
            ▶
          </button>
        </div>

        <div className="virtual-center-buttons">
          <button
            className={`virtual-center-btn ${isActive('select') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('select', e)}
            onMouseUp={(e) => handleButtonUp('select', e)}
            onMouseLeave={(e) => handleButtonUp('select', e)}
            onTouchStart={(e) => handleButtonDown('select', e)}
            onTouchEnd={(e) => handleButtonUp('select', e)}
          >
            SELECT
          </button>
          <button
            className={`virtual-center-btn ${isActive('start') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('start', e)}
            onMouseUp={(e) => handleButtonUp('start', e)}
            onMouseLeave={(e) => handleButtonUp('start', e)}
            onTouchStart={(e) => handleButtonDown('start', e)}
            onTouchEnd={(e) => handleButtonUp('start', e)}
          >
            START
          </button>
        </div>

        <div className="virtual-action-buttons">
          <button
            className={`virtual-action-btn ${isActive('b') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('b', e)}
            onMouseUp={(e) => handleButtonUp('b', e)}
            onMouseLeave={(e) => handleButtonUp('b', e)}
            onTouchStart={(e) => handleButtonDown('b', e)}
            onTouchEnd={(e) => handleButtonUp('b', e)}
          >
            B
          </button>
          <button
            className={`virtual-action-btn ${isActive('a') ? 'active' : ''}`}
            onMouseDown={(e) => handleButtonDown('a', e)}
            onMouseUp={(e) => handleButtonUp('a', e)}
            onMouseLeave={(e) => handleButtonUp('a', e)}
            onTouchStart={(e) => handleButtonDown('a', e)}
            onTouchEnd={(e) => handleButtonUp('a', e)}
          >
            A
          </button>
        </div>
      </div>
    </div>
  );
}
