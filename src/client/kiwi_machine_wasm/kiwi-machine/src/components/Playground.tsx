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
import {Dispatch, RefObject, SetStateAction, useEffect, useRef, useState} from "react";
import VirtualController, {ControllerButton} from "./basic/VirtualController";
import ControlPanel from "./ControlPanel";
import {CreateEmulatorService} from "../services/emulator";
import {isMobileDevice} from "../services/device";
import LoadingSplash from "./LoadingSplash";

interface PlaygroundProps {
  setFrameRef: Dispatch<SetStateAction<RefObject<HTMLIFrameElement>>>,
  showManualModal: boolean,
  showAboutModal: boolean,
  showSaveLoadModal: boolean,
  setShowManualModal: Dispatch<SetStateAction<boolean>>,
  setShowAboutModal: Dispatch<SetStateAction<boolean>>,
  setShowSaveLoadModal: Dispatch<SetStateAction<boolean>>,
  showToast: boolean,
  setShowToast: Dispatch<SetStateAction<boolean>>,
  toastMessage: string,
  setToastMessage: Dispatch<SetStateAction<string>>,
  onEmulatorReady?: () => void
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

export default function Playground({setFrameRef, showManualModal, showAboutModal, showSaveLoadModal, setShowManualModal, setShowAboutModal, setShowSaveLoadModal, showToast, setShowToast, toastMessage, setToastMessage, onEmulatorReady}: PlaygroundProps) {
  const frameRef = useRef<HTMLIFrameElement>(null);
  const [showFps, setShowFps] = useState(false);
  const [showControl, setShowControl] = useState(false);
  const [isSplashFinished, setIsSplashFinished] = useState(false);
  const [showCanvas, setShowCanvas] = useState(false);
  const isMobile = isMobileDevice();

  useEffect(() => {
    setFrameRef(frameRef);
  }, [setFrameRef]);

  const focusIframe = () => {
    setTimeout(() => {
      frameRef.current?.focus();
    }, 100);
  };

  const saveToAutoSlot = () => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      const emulatorService = CreateEmulatorService(currentWindow);
      const count = emulatorService.getSaveStatesCount();
      if (count > 0) {
        emulatorService.saveState(count - 1);
      }
    }
  };

  const loadFromAutoSlot = () => {
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      const emulatorService = CreateEmulatorService(currentWindow);
      const count = emulatorService.getSaveStatesCount();
      if (count > 0) {
        emulatorService.loadState(count - 1);
      }
    }
  };

  useEffect(() => {
    const handleEscape = () => {
      if (showSaveLoadModal) {
        setShowSaveLoadModal(false);
      } else if (showManualModal) {
        setShowManualModal(false);
      } else if (showAboutModal) {
        setShowAboutModal(false);
      } else {
        setShowControl(!showControl);
      }
      
      focusIframe();
    };

    const saveToAutoSlotLocal = () => {
      const currentWindow = frameRef.current?.contentWindow;
      if (currentWindow) {
        const emulatorService = CreateEmulatorService(currentWindow);
        const count = emulatorService.getSaveStatesCount();
        if (count > 0) {
          emulatorService.saveState(count - 1);
        }
      }
    };

    const loadFromAutoSlotLocal = () => {
      const currentWindow = frameRef.current?.contentWindow;
      if (currentWindow) {
        const emulatorService = CreateEmulatorService(currentWindow);
        const count = emulatorService.getSaveStatesCount();
        if (count > 0) {
          emulatorService.loadState(count - 1);
        }
      }
    };

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        handleEscape();
      } else if (event.key === 'F1') {
        event.preventDefault();
        saveToAutoSlotLocal();
      } else if (event.key === 'F2') {
        event.preventDefault();
        loadFromAutoSlotLocal();
      }
    };

    window.addEventListener('keydown', handleKeyDown);

    const handleIframeMessage = (event: MessageEvent) => {
      if (event.data && event.data.type === 'escapeKeyDown') {
        handleEscape();
      } else if (event.data && event.data.type === 'saveStateShortcut') {
        saveToAutoSlotLocal();
      } else if (event.data && event.data.type === 'loadStateShortcut') {
        loadFromAutoSlotLocal();
      }
    };

    window.addEventListener('message', handleIframeMessage);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('message', handleIframeMessage);
    };
  }, [showControl, showManualModal, showAboutModal, showSaveLoadModal, setShowAboutModal, setShowManualModal, setShowSaveLoadModal]);

  useEffect(() => {
    const handleIframeLoad = () => {
      const iframeWindow = frameRef.current?.contentWindow;
      if (iframeWindow) {
        (iframeWindow as any).KiwiMachineCallback = {
          onSplashFinished: () => {
            setIsSplashFinished(true);
            if (onEmulatorReady) {
              onEmulatorReady();
            }
          },
          onVolumeChanged: (data: { volume: number }) => {
          },
          onSaveStateSucceeded: (slot: number) => {
            setToastMessage('保存成功');
            setShowToast(true);
          },
          onSaveStateFailed: (slot: number) => {
            setToastMessage('保存失败');
            setShowToast(true);
          },
          onLoadStateSucceeded: (slot: number) => {
            setToastMessage('读取成功');
            setShowToast(true);
          },
          onLoadStateFailed: (slot: number) => {
            setToastMessage('读取失败');
            setShowToast(true);
          }
        };
      }
    };

    const iframe = frameRef.current;
    if (iframe) {
      iframe.addEventListener('load', handleIframeLoad);
    }

    return () => {
      if (iframe) {
        iframe.removeEventListener('load', handleIframeLoad);
      }
    };
  }, [onEmulatorReady, setShowToast, setToastMessage]);

  const handleSplashFinished = () => {
    setShowCanvas(true);
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
    <div className={`playground ${isMobile ? 'playground-mobile' : 'playground-desktop'}`}>
      <LoadingSplash 
        onFinished={handleSplashFinished} 
        isReady={isSplashFinished} 
      />
      
      <iframe 
        className={`playground-frame ${showCanvas ? 'playground-frame-visible' : 'playground-frame-hidden'}`} 
        ref={frameRef} 
        src="kiwi_machine.html" 
        title="Kiwi Machine"
        tabIndex={0}
      />

      <VirtualController 
        onButtonPress={handleButtonPress} 
        onButtonRelease={handleButtonRelease}
        onMenuButtonClick={() => setShowControl(!showControl)}
      />

      {!isMobileDevice() && <div className="playground-float-group">
        <button
          className="playground-float-button playground-float-button-small"
          title="存档 (F1)"
          onClick={() => { saveToAutoSlot(); focusIframe(); }}
        >
          <svg className="playground-float-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path>
            <line x1="12" y1="7" x2="12" y2="14"></line>
            <polyline points="9 11 12 14 15 11"></polyline>
          </svg>
        </button>
        <button
          className="playground-float-button playground-float-button-small"
          title="读档 (F2)"
          onClick={() => { loadFromAutoSlot(); focusIframe(); }}
        >
          <svg className="playground-float-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path>
            <line x1="12" y1="7" x2="12" y2="14"></line>
            <polyline points="9 10 12 7 15 10"></polyline>
          </svg>
        </button>
        <div className="playground-float-button" onClick={() => setShowControl(!showControl)}>
          <svg className="playground-float-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="3"></circle>
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06-.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
          </svg>
        </div>
      </div>}

      <ControlPanel
        show={showControl}
        setVisible={setShowControl}
        frameRef={frameRef}
        setShowSaveLoadModal={setShowSaveLoadModal}
        setShowManualModal={setShowManualModal}
        setShowAboutModal={setShowAboutModal}
        showFps={showFps}
        setShowFps={setShowFps}
        onClose={focusIframe}
      />
    </div>
  );
}
