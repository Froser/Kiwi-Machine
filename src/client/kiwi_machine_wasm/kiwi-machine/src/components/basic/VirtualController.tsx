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
import { useState, useCallback, useRef, useEffect } from "react";

export type ControllerButton = 'up' | 'down' | 'left' | 'right' | 'a' | 'b' | 'select' | 'start';
type DiagonalButton = 'up-left' | 'up-right' | 'down-left' | 'down-right';
type AllButtonType = ControllerButton | DiagonalButton;

interface VirtualControllerProps {
  onButtonPress?: (button: ControllerButton) => void;
  onButtonRelease?: (button: ControllerButton) => void;
}

const diagonalButtonMap: Record<DiagonalButton, ControllerButton[]> = {
  'up-left': ['up', 'left'],
  'up-right': ['up', 'right'],
  'down-left': ['down', 'left'],
  'down-right': ['down', 'right'],
};

const allButtons: AllButtonType[] = ['up', 'down', 'left', 'right', 'a', 'b', 'select', 'start', 'up-left', 'up-right', 'down-left', 'down-right'];

export default function VirtualController({ onButtonPress, onButtonRelease }: VirtualControllerProps) {
  const [activeButtons, setActiveButtons] = useState<Set<ControllerButton>>(new Set());
  const touchToButtonMap = useRef<Map<number, AllButtonType>>(new Map());
  const buttonRefs = useRef<Record<AllButtonType, HTMLButtonElement | null>>({
    up: null,
    down: null,
    left: null,
    right: null,
    a: null,
    b: null,
    select: null,
    start: null,
    'up-left': null,
    'up-right': null,
    'down-left': null,
    'down-right': null,
  });
  const containerRef = useRef<HTMLDivElement>(null);

  const getButtonAtPosition = useCallback((clientX: number, clientY: number): AllButtonType | null => {
    for (const button of allButtons) {
      const element = buttonRefs.current[button];
      if (element) {
        const rect = element.getBoundingClientRect();
        if (clientX >= rect.left && clientX <= rect.right &&
            clientY >= rect.top && clientY <= rect.bottom) {
          return button;
        }
      }
    }
    return null;
  }, []);

  const setButtonActive = useCallback((button: AllButtonType) => {
    if (button in diagonalButtonMap) {
      const buttons = diagonalButtonMap[button as DiagonalButton];
      setActiveButtons(prev => {
        const next = new Set(prev);
        buttons.forEach(b => {
          if (!next.has(b)) {
            onButtonPress?.(b);
            next.add(b);
          }
        });
        return next;
      });
    } else {
      setActiveButtons(prev => {
        if (!prev.has(button as ControllerButton)) {
          onButtonPress?.(button as ControllerButton);
          return new Set(prev).add(button as ControllerButton);
        }
        return prev;
      });
    }
  }, [onButtonPress]);

  const setButtonInactive = useCallback((button: AllButtonType) => {
    if (button in diagonalButtonMap) {
      const buttons = diagonalButtonMap[button as DiagonalButton];
      setActiveButtons(prev => {
        const next = new Set(prev);
        buttons.forEach(b => {
          if (next.has(b)) {
            onButtonRelease?.(b);
            next.delete(b);
          }
        });
        return next;
      });
    } else {
      setActiveButtons(prev => {
        if (prev.has(button as ControllerButton)) {
          onButtonRelease?.(button as ControllerButton);
          const next = new Set(prev);
          next.delete(button as ControllerButton);
          return next;
        }
        return prev;
      });
    }
  }, [onButtonRelease]);

  const handleTouchStart = useCallback((e: React.TouchEvent) => {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      const touch = e.changedTouches[i];
      const buttonAtPos = getButtonAtPosition(touch.clientX, touch.clientY);
      if (buttonAtPos) {
        touchToButtonMap.current.set(touch.identifier, buttonAtPos);
        setButtonActive(buttonAtPos);
      }
    }
  }, [getButtonAtPosition, setButtonActive]);

  const handleTouchMove = useCallback((e: React.TouchEvent) => {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      const touch = e.changedTouches[i];
      const oldButton = touchToButtonMap.current.get(touch.identifier);
      const newButton = getButtonAtPosition(touch.clientX, touch.clientY);

      if (oldButton !== newButton) {
        if (oldButton) {
          setButtonInactive(oldButton);
          touchToButtonMap.current.delete(touch.identifier);
        }
        if (newButton) {
          touchToButtonMap.current.set(touch.identifier, newButton);
          setButtonActive(newButton);
        }
      }
    }
  }, [getButtonAtPosition, setButtonActive, setButtonInactive]);

  const handleTouchEnd = useCallback((e: React.TouchEvent) => {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      const touch = e.changedTouches[i];
      const button = touchToButtonMap.current.get(touch.identifier);
      if (button) {
        setButtonInactive(button);
        touchToButtonMap.current.delete(touch.identifier);
      }
    }
  }, [setButtonInactive]);

  const handleTouchCancel = useCallback((e: React.TouchEvent) => {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      const touch = e.changedTouches[i];
      const button = touchToButtonMap.current.get(touch.identifier);
      if (button) {
        setButtonInactive(button);
        touchToButtonMap.current.delete(touch.identifier);
      }
    }
  }, [setButtonInactive]);

  const handleMouseDown = useCallback((button: AllButtonType, e: React.MouseEvent) => {
    e.preventDefault();
    setButtonActive(button);
  }, [setButtonActive]);

  const handleMouseUp = useCallback((button: AllButtonType) => {
    setButtonInactive(button);
  }, [setButtonInactive]);

  const isActive = (button: ControllerButton) => activeButtons.has(button);

  useEffect(() => {
    const handleGlobalMouseUp = () => {
      setActiveButtons(prev => {
        prev.forEach(button => {
          onButtonRelease?.(button);
        });
        return new Set();
      });
    };

    window.addEventListener('mouseup', handleGlobalMouseUp);

    return () => {
      window.removeEventListener('mouseup', handleGlobalMouseUp);
    };
  }, [onButtonRelease]);

  return (
    <div 
      className="virtual-controller"
      ref={containerRef}
      onTouchStart={handleTouchStart}
      onTouchMove={handleTouchMove}
      onTouchEnd={handleTouchEnd}
      onTouchCancel={handleTouchCancel}
    >
      <div className="virtual-controller-wrapper">
        <div className="virtual-top-row">
          <div className="virtual-dpad">
            <div className="virtual-dpad-center"></div>
            <button
              ref={(el) => buttonRefs.current['up-left'] = el}
              className="virtual-dpad-diagonal virtual-dpad-up-left"
              onMouseDown={(e) => handleMouseDown('up-left', e)}
              onMouseUp={() => handleMouseUp('up-left')}
              onMouseLeave={() => handleMouseUp('up-left')}
            ></button>
            <button
              ref={(el) => buttonRefs.current['up-right'] = el}
              className="virtual-dpad-diagonal virtual-dpad-up-right"
              onMouseDown={(e) => handleMouseDown('up-right', e)}
              onMouseUp={() => handleMouseUp('up-right')}
              onMouseLeave={() => handleMouseUp('up-right')}
            ></button>
            <button
              ref={(el) => buttonRefs.current['down-left'] = el}
              className="virtual-dpad-diagonal virtual-dpad-down-left"
              onMouseDown={(e) => handleMouseDown('down-left', e)}
              onMouseUp={() => handleMouseUp('down-left')}
              onMouseLeave={() => handleMouseUp('down-left')}
            ></button>
            <button
              ref={(el) => buttonRefs.current['down-right'] = el}
              className="virtual-dpad-diagonal virtual-dpad-down-right"
              onMouseDown={(e) => handleMouseDown('down-right', e)}
              onMouseUp={() => handleMouseUp('down-right')}
              onMouseLeave={() => handleMouseUp('down-right')}
            ></button>
            <button
              ref={(el) => buttonRefs.current.up = el}
              className={`virtual-dpad-btn virtual-dpad-up ${isActive('up') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('up', e)}
              onMouseUp={() => handleMouseUp('up')}
              onMouseLeave={() => handleMouseUp('up')}
            >
              ▲
            </button>
            <button
              ref={(el) => buttonRefs.current.down = el}
              className={`virtual-dpad-btn virtual-dpad-down ${isActive('down') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('down', e)}
              onMouseUp={() => handleMouseUp('down')}
              onMouseLeave={() => handleMouseUp('down')}
            >
              ▼
            </button>
            <button
              ref={(el) => buttonRefs.current.left = el}
              className={`virtual-dpad-btn virtual-dpad-left ${isActive('left') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('left', e)}
              onMouseUp={() => handleMouseUp('left')}
              onMouseLeave={() => handleMouseUp('left')}
            >
              ◀
            </button>
            <button
              ref={(el) => buttonRefs.current.right = el}
              className={`virtual-dpad-btn virtual-dpad-right ${isActive('right') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('right', e)}
              onMouseUp={() => handleMouseUp('right')}
              onMouseLeave={() => handleMouseUp('right')}
            >
              ▶
            </button>
          </div>

          <div className="virtual-action-buttons">
            <button
              ref={(el) => buttonRefs.current.b = el}
              className={`virtual-action-btn ${isActive('b') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('b', e)}
              onMouseUp={() => handleMouseUp('b')}
              onMouseLeave={() => handleMouseUp('b')}
            >
              B
            </button>
            <button
              ref={(el) => buttonRefs.current.a = el}
              className={`virtual-action-btn ${isActive('a') ? 'active' : ''}`}
              onMouseDown={(e) => handleMouseDown('a', e)}
              onMouseUp={() => handleMouseUp('a')}
              onMouseLeave={() => handleMouseUp('a')}
            >
              A
            </button>
          </div>
        </div>

        <div className="virtual-center-buttons">
          <button
            ref={(el) => buttonRefs.current.select = el}
            className={`virtual-center-btn ${isActive('select') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('select', e)}
            onMouseUp={() => handleMouseUp('select')}
            onMouseLeave={() => handleMouseUp('select')}
          >
            SELECT
          </button>
          <button
            ref={(el) => buttonRefs.current.start = el}
            className={`virtual-center-btn ${isActive('start') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('start', e)}
            onMouseUp={() => handleMouseUp('start')}
            onMouseLeave={() => handleMouseUp('start')}
          >
            START
          </button>
        </div>
      </div>
    </div>
  );
}
