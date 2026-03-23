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

class EmulatorService {
  private window: Window;
  constructor(window: Window) {
    this.window = window;
  }

  setVolume(volume: number) {
    this.window.postMessage({
      type: 'changeVolume',
      data: {
        volume: volume
      }
    });
  }

  loadROM(romUrl: string) {
    this.window.postMessage(
      {
        type: 'loadROMBinary',
        data: romUrl
      }
    );
  }

  callMenu() {
    this.window.postMessage(
      {
        type: 'callMenu',
      }
    );
  }

  joystickButtonDown(keyCode: number) {
    this.window.postMessage({
      type: 'joystickButtonDown',
      data: {
        keyCode: keyCode
      }
    });
  }

  joystickButtonUp(keyCode: number) {
    this.window.postMessage({
      type: 'joystickButtonUp',
      data: {
        keyCode: keyCode
      }
    });
  }

  saveState(slot: number) {
    this.window.postMessage({
      type: 'saveState',
      data: {
        slot: slot
      }
    });
  }

  loadState(slot: number) {
    this.window.postMessage({
      type: 'loadState',
      data: {
        slot: slot
      }
    });
  }

  getSaveStatesCount(): number {
    const module = (this.window as any).Module;
    if (module && module._GetSaveStatesCount) {
      return module._GetSaveStatesCount();
    }
    return 0;
  }

  hasSaveState(slot: number): boolean {
    const module = (this.window as any).Module;
    if (module && module._HasSaveState) {
      return module._HasSaveState(slot);
    }
    return false;
  }

  getSaveStateThumbnail(slot: number): string {
    const module = (this.window as any).Module;
    if (module && module._GetSaveStateThumbnail) {
      const ptr = module._GetSaveStateThumbnail(slot);
      if (ptr) {
        return module.UTF8ToString(ptr);
      }
    }
    return '';
  }
}

function CreateEmulatorService(window: Window | null | undefined) {
  if (!window)
    throw new Error("Window is null");
  
  return new EmulatorService(window);
}

export {
  CreateEmulatorService
}