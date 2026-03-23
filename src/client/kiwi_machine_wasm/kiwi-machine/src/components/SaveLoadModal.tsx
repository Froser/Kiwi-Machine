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

import "./SaveLoadModal.css"
import Modal from "./basic/Modal";
import Button from "./basic/Button";
import {Dispatch, RefObject, SetStateAction, useEffect, useState} from "react";
import {CreateEmulatorService} from "../services/emulator";

interface SaveLoadModalProps {
  show: boolean,
  setVisible: Dispatch<SetStateAction<boolean>>,
  frameRef: RefObject<HTMLIFrameElement>
}

interface SaveSlot {
  slot: number,
  hasData: boolean,
  thumbnail: string
}

export default function SaveLoadModal({show, setVisible, frameRef}: SaveLoadModalProps) {
  const [saveSlots, setSaveSlots] = useState<SaveSlot[]>([]);

  useEffect(() => {
    if (show) {
      loadSaveSlots();
    }
  }, [show]);

  const loadSaveSlots = () => {
    const currentWindow = frameRef.current?.contentWindow;
    if (!currentWindow) return;

    const emulatorService = CreateEmulatorService(currentWindow);
    const count = emulatorService.getSaveStatesCount();
    const slots: SaveSlot[] = [];

    for (let i = 0; i < count; i++) {
      slots.push({
        slot: i,
        hasData: emulatorService.hasSaveState(i),
        thumbnail: emulatorService.getSaveStateThumbnail(i)
      });
    }

    setSaveSlots(slots);
  };

  const handleSave = (slot: number) => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      CreateEmulatorService(currentWindow).saveState(slot);
      setTimeout(() => {
        loadSaveSlots();
      }, 500);
    }
  };

  const handleLoad = (slot: number) => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      CreateEmulatorService(currentWindow).loadState(slot);
      setVisible(false);
    }
  };

  return (
    <Modal title="存档/读档" show={show} setVisible={setVisible} width="800px" height="auto">
      <div className="save-load-modal">
        <div className="save-load-grid">
          {saveSlots.map((slot) => (
            <div key={slot.slot} className="save-slot">
              <div className="save-slot-number">存档 {slot.slot + 1}</div>
              <div className="save-slot-thumbnail">
                {slot.hasData && slot.thumbnail ? (
                  <img src={slot.thumbnail} alt={`存档 ${slot.slot + 1}`} />
                ) : (
                  <div className="save-slot-empty">空</div>
                )}
              </div>
              <div className="save-slot-actions">
                <Button text="保存" onClick={() => handleSave(slot.slot)} />
                {slot.hasData && (
                  <Button text="读取" onClick={() => handleLoad(slot.slot)} />
                )}
              </div>
            </div>
          ))}
        </div>
      </div>
    </Modal>
  );
}
