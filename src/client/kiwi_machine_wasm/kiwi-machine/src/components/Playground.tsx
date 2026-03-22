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

import "./Playground.css"
import Button from "./basic/Button";
import Checkbox from "./basic/Checkbox";
import {Dispatch, RefObject, SetStateAction, useEffect, useRef, useState} from "react";
import VirtualController, {ControllerButton} from "./basic/VirtualController";
import VolumePanel from "./VolumePanel";
import {CreateEmulatorService} from "../services/emulator";
import {isMobileDevice} from "../services/device";

interface PlaygroundProps {
  setFrameRef: Dispatch<SetStateAction<RefObject<HTMLIFrameElement>>>,
  setShowManualModal: Dispatch<SetStateAction<boolean>>,
  setShowAboutModal: Dispatch<SetStateAction<boolean>>
}

const controllerButtonToJoystickButton: Record<ControllerButton, number> = {
  a: 0,
  b: 1,
  select: 2,
  start: 3,
  up: 4,
  down: 5,
  left: 6,
  right: 7
};

export default function Playground({setFrameRef, setShowManualModal, setShowAboutModal}: PlaygroundProps) {
  const frameRef = useRef<HTMLIFrameElement>(null);
  const [showFps, setShowFps] = useState(false);
  const [showControl, setShowControl] = useState(false);

  useEffect(() => {
    setFrameRef(frameRef);
  }, [setFrameRef]);

  const handleShowFpsChange = (checked: boolean) => {
    setShowFps(checked);
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      currentWindow.postMessage({
        type: 'toggleFps',
        data: { show: checked }
      }, '*');
    }
  };

  const handleButtonPress = (button: ControllerButton) => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      CreateEmulatorService(currentWindow).joystickButtonDown(controllerButtonToJoystickButton[button]);
    }
  };

  const handleButtonRelease = (button: ControllerButton) => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      CreateEmulatorService(currentWindow).joystickButtonUp(controllerButtonToJoystickButton[button]);
    }
  };

  return (
    <div className="playground">
      <iframe className="playground-frame" ref={frameRef} src="kiwi_machine.html" title="Kiwi Machine">
      </iframe>

      <VirtualController onButtonPress={handleButtonPress} onButtonRelease={handleButtonRelease} />

      <div className="playground-float-button" onClick={() => setShowControl(!showControl)}>
        <svg className="playground-float-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <circle cx="12" cy="12" r="3"></circle>
          <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
        </svg>
      </div>

      {showControl && <div className='playground-control'>
        <div className="playground-control-header">
          <span className="playground-control-title">控制面板</span>
          <button className="playground-control-close" onClick={() => setShowControl(false)}>✕</button>
        </div>
        <div className="playground-control-content">
          <div className="playground-control-group">
            <div className="playground-control-row">
              <Button text="游戏菜单 (ESC)" onClick={() => {
                const currentWindow = frameRef.current?.contentWindow;
                CreateEmulatorService(currentWindow).callMenu();
                currentWindow?.focus();
              }}/>
              <VolumePanel id='volume_slider' frame={frameRef}/>
            </div>
            <div className="playground-control-row">
              <Button text="操作说明" onClick={() => setShowManualModal(true)}/>
              <Button text="关于Kiwi Machine" onClick={() => setShowAboutModal(true)}/>
            </div>
            <div className="playground-control-row playground-control-row-single">
              <Checkbox 
                id="showFpsCheckbox"
                label="显示帧率" 
                checked={showFps} 
                onChange={handleShowFpsChange}
              />
            </div>
          </div>
        </div>
      </div>}
    </div>
  );
}