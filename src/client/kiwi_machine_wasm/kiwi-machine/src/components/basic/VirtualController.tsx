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
  const [isPointerDown, setIsPointerDown] = useState(false);
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

  const setAllButtonsInactive = useCallback(() => {
    setActiveButtons(prev => {
      prev.forEach(button => {
        onButtonRelease?.(button);
      });
      return new Set();
    });
  }, [onButtonRelease]);

  const handlePointerMove = useCallback((clientX: number, clientY: number) => {
    if (!isPointerDown) return;

    const buttonAtPos = getButtonAtPosition(clientX, clientY);
    
    const getButtonsToRelease = (button: AllButtonType | null): ControllerButton[] => {
      if (!button) return [];
      if (button in diagonalButtonMap) {
        return diagonalButtonMap[button as DiagonalButton];
      }
      return [button as ControllerButton];
    };

    const currentButtons = getButtonsToRelease(buttonAtPos);
    
    activeButtons.forEach(button => {
      if (!currentButtons.includes(button)) {
        setButtonInactive(button);
      }
    });

    if (buttonAtPos) {
      setButtonActive(buttonAtPos);
    }
  }, [isPointerDown, activeButtons, getButtonAtPosition, setButtonActive, setButtonInactive]);

  const handleMouseDown = useCallback((button: AllButtonType, e: React.MouseEvent) => {
    e.preventDefault();
    setIsPointerDown(true);
    setButtonActive(button);
  }, [setButtonActive]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    handlePointerMove(e.clientX, e.clientY);
  }, [handlePointerMove]);

  const handleMouseUp = useCallback(() => {
    setIsPointerDown(false);
    setAllButtonsInactive();
  }, [setAllButtonsInactive]);

  const handleTouchStart = useCallback((button: AllButtonType, e: React.TouchEvent) => {
    e.preventDefault();
    setIsPointerDown(true);
    setButtonActive(button);
  }, [setButtonActive]);

  const handleTouchMove = useCallback((e: React.TouchEvent) => {
    if (e.touches.length > 0) {
      const touch = e.touches[0];
      handlePointerMove(touch.clientX, touch.clientY);
    }
  }, [handlePointerMove]);

  const handleTouchEnd = useCallback(() => {
    setIsPointerDown(false);
    setAllButtonsInactive();
  }, [setAllButtonsInactive]);

  const isActive = (button: ControllerButton) => activeButtons.has(button);

  useEffect(() => {
    const handleGlobalMouseUp = () => {
      if (isPointerDown) {
        setIsPointerDown(false);
        setAllButtonsInactive();
      }
    };

    window.addEventListener('mouseup', handleGlobalMouseUp);
    window.addEventListener('touchend', handleGlobalMouseUp);

    return () => {
      window.removeEventListener('mouseup', handleGlobalMouseUp);
      window.removeEventListener('touchend', handleGlobalMouseUp);
    };
  }, [isPointerDown, setAllButtonsInactive]);

  return (
    <div 
      className="virtual-controller"
      ref={containerRef}
      onMouseMove={handleMouseMove}
      onTouchMove={handleTouchMove}
      onMouseUp={handleMouseUp}
      onTouchEnd={handleTouchEnd}
    >
      <div className="virtual-controller-wrapper">
        <div className="virtual-dpad">
          <div className="virtual-dpad-center"></div>
          <button
            ref={(el) => buttonRefs.current['up-left'] = el}
            className="virtual-dpad-diagonal virtual-dpad-up-left"
            onMouseDown={(e) => handleMouseDown('up-left', e)}
            onTouchStart={(e) => handleTouchStart('up-left', e)}
          ></button>
          <button
            ref={(el) => buttonRefs.current['up-right'] = el}
            className="virtual-dpad-diagonal virtual-dpad-up-right"
            onMouseDown={(e) => handleMouseDown('up-right', e)}
            onTouchStart={(e) => handleTouchStart('up-right', e)}
          ></button>
          <button
            ref={(el) => buttonRefs.current['down-left'] = el}
            className="virtual-dpad-diagonal virtual-dpad-down-left"
            onMouseDown={(e) => handleMouseDown('down-left', e)}
            onTouchStart={(e) => handleTouchStart('down-left', e)}
          ></button>
          <button
            ref={(el) => buttonRefs.current['down-right'] = el}
            className="virtual-dpad-diagonal virtual-dpad-down-right"
            onMouseDown={(e) => handleMouseDown('down-right', e)}
            onTouchStart={(e) => handleTouchStart('down-right', e)}
          ></button>
          <button
            ref={(el) => buttonRefs.current.up = el}
            className={`virtual-dpad-btn virtual-dpad-up ${isActive('up') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('up', e)}
            onTouchStart={(e) => handleTouchStart('up', e)}
          >
            ▲
          </button>
          <button
            ref={(el) => buttonRefs.current.down = el}
            className={`virtual-dpad-btn virtual-dpad-down ${isActive('down') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('down', e)}
            onTouchStart={(e) => handleTouchStart('down', e)}
          >
            ▼
          </button>
          <button
            ref={(el) => buttonRefs.current.left = el}
            className={`virtual-dpad-btn virtual-dpad-left ${isActive('left') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('left', e)}
            onTouchStart={(e) => handleTouchStart('left', e)}
          >
            ◀
          </button>
          <button
            ref={(el) => buttonRefs.current.right = el}
            className={`virtual-dpad-btn virtual-dpad-right ${isActive('right') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('right', e)}
            onTouchStart={(e) => handleTouchStart('right', e)}
          >
            ▶
          </button>
        </div>

        <div className="virtual-center-buttons">
          <button
            ref={(el) => buttonRefs.current.select = el}
            className={`virtual-center-btn ${isActive('select') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('select', e)}
            onTouchStart={(e) => handleTouchStart('select', e)}
          >
            SELECT
          </button>
          <button
            ref={(el) => buttonRefs.current.start = el}
            className={`virtual-center-btn ${isActive('start') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('start', e)}
            onTouchStart={(e) => handleTouchStart('start', e)}
          >
            START
          </button>
        </div>

        <div className="virtual-action-buttons">
          <button
            ref={(el) => buttonRefs.current.b = el}
            className={`virtual-action-btn ${isActive('b') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('b', e)}
            onTouchStart={(e) => handleTouchStart('b', e)}
          >
            B
          </button>
          <button
            ref={(el) => buttonRefs.current.a = el}
            className={`virtual-action-btn ${isActive('a') ? 'active' : ''}`}
            onMouseDown={(e) => handleMouseDown('a', e)}
            onTouchStart={(e) => handleTouchStart('a', e)}
          >
            A
          </button>
        </div>
      </div>
    </div>
  );
}
