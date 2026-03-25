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
import {Dispatch, RefObject, SetStateAction, useCallback, useEffect, useState} from "react";
import {CreateEmulatorService} from "../services/emulator";

interface SaveLoadModalProps {
  show: boolean,
  setVisible: Dispatch<SetStateAction<boolean>>,
  frameRef: RefObject<HTMLIFrameElement>
}

interface SaveSlot {
  slot: number,
  hasData: boolean,
  thumbnail: string,
  processedThumbnail?: string
}

export default function SaveLoadModal({show, setVisible, frameRef}: SaveLoadModalProps) {
  const [saveSlots, setSaveSlots] = useState<SaveSlot[]>([]);

  const loadSaveSlots = useCallback(() => {
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
  }, [frameRef]);

  useEffect(() => {
    if (show) {
      loadSaveSlots();
    }
  }, [show, loadSaveSlots]);

  const processThumbnail = useCallback((thumbnail: string): Promise<string> => {
    return new Promise((resolve) => {
      const img = new Image();
      img.crossOrigin = "anonymous";
      img.onload = () => {
        const canvas = document.createElement('canvas');
        canvas.width = img.width;
        canvas.height = img.height;
        const ctx = canvas.getContext('2d');
        if (ctx) {
          ctx.drawImage(img, 0, 0);
          const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
          const data = imageData.data;
          
          for (let i = 0; i < data.length; i += 4) {
            const r = data[i];
            const b = data[i + 2];
            data[i] = b;
            data[i + 2] = r;
          }
          
          ctx.putImageData(imageData, 0, 0);
          resolve(canvas.toDataURL('image/jpeg'));
        } else {
          resolve(thumbnail);
        }
      };
      img.onerror = () => resolve(thumbnail);
      img.src = thumbnail;
    });
  }, []);

  useEffect(() => {
    const processAllThumbnails = async () => {
      const processedSlots = await Promise.all(saveSlots.map(async (slot) => {
        if (slot.hasData && slot.thumbnail && !slot.processedThumbnail) {
          try {
            const processed = await processThumbnail(slot.thumbnail);
            return { ...slot, processedThumbnail: processed };
          } catch (e) {
            return slot;
          }
        }
        return slot;
      }));
      setSaveSlots(processedSlots);
    };
    if (saveSlots.length > 0) {
      processAllThumbnails();
    }
  }, [saveSlots, processThumbnail]);

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

  const handleDelete = (slot: number) => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      CreateEmulatorService(currentWindow).deleteState(slot);
      setTimeout(() => {
        loadSaveSlots();
      }, 500);
    }
  };

  return (
    <Modal title="存档/读档" show={show} setVisible={setVisible} width="1100px" height="auto">
      <div className="save-load-modal">
        <div className="save-load-grid">
          {saveSlots.map((slot) => {
            const slotNumber = slot.slot + 1;
            const thumbnailToUse = slot.processedThumbnail || slot.thumbnail;
            return (
            <div key={slot.slot} className="save-slot">
              <div className="save-slot-number">存档 {slotNumber}</div>
              <div className="save-slot-thumbnail">
                {slot.hasData && thumbnailToUse ? (
                  <img src={thumbnailToUse} alt={`存档 ${slotNumber}`} />
                ) : (
                  <div className="save-slot-empty">空</div>
                )}
              </div>
              <div className="save-slot-actions">
                <button className="button" onClick={() => handleSave(slot.slot)}>保存</button>
                {slot.hasData ? (
                  <>
                    <button className="button" onClick={() => handleLoad(slot.slot)}>读取</button>
                    <button className="button button-danger" onClick={() => handleDelete(slot.slot)}>删除</button>
                  </>
                ) : null}
              </div>
            </div>
            );
          })}
        </div>
      </div>
    </Modal>
  );
}
