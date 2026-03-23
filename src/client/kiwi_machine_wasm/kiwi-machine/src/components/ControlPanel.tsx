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

import "./ControlPanel.css"
import Button from "./basic/Button";
import Checkbox from "./basic/Checkbox";
import VolumePanel from "./VolumePanel";
import {Dispatch, RefObject, SetStateAction} from "react";
import {CreateEmulatorService} from "../services/emulator";

interface ControlPanelProps {
  show: boolean;
  setVisible: Dispatch<SetStateAction<boolean>>;
  frameRef: RefObject<HTMLIFrameElement>;
  setShowSaveLoadModal: Dispatch<SetStateAction<boolean>>;
  setShowManualModal: Dispatch<SetStateAction<boolean>>;
  setShowAboutModal: Dispatch<SetStateAction<boolean>>;
  showFps: boolean;
  setShowFps: Dispatch<SetStateAction<boolean>>;
}

export default function ControlPanel({
  show,
  setVisible,
  frameRef,
  setShowSaveLoadModal,
  setShowManualModal,
  setShowAboutModal,
  showFps,
  setShowFps
}: ControlPanelProps) {
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

  if (!show) {
    return null;
  }

  return (
    <div className="control-panel-overlay" onClick={() => setVisible(false)}>
      <div className='control-panel' onClick={(event) => event.stopPropagation()}>
        <div className="control-panel-header">
          <span className="control-panel-title">游戏菜单</span>
          <button className="control-panel-close" onClick={() => setVisible(false)}>✕</button>
        </div>
        <div className="control-panel-content">
          <div className="control-panel-group">
            <div className="control-panel-row">
              <Button text="存档/读档" onClick={() => setShowSaveLoadModal(true)}/>
              <Button text="重置" onClick={() => {
                const currentWindow = frameRef.current?.contentWindow;
                if (currentWindow) {
                  CreateEmulatorService(currentWindow).resetROM();
                  currentWindow.focus();
                }
              }}/>
            </div>
            <div className="control-panel-row">
              <VolumePanel id='volume_slider' frame={frameRef}/>
            </div>
            <div className="control-panel-row">
              <Button text="操作说明" onClick={() => setShowManualModal(true)}/>
              <Button text="关于Kiwi Machine" onClick={() => setShowAboutModal(true)}/>
            </div>
            <div className="control-panel-row control-panel-row-single">
              <Checkbox 
                id="showFpsCheckbox"
                label="显示帧率" 
                checked={showFps} 
                onChange={handleShowFpsChange}
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
