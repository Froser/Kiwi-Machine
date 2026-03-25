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
  setShowSaveLoadModal: Dispatch<SetStateAction<boolean>>
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

export default function Playground({setFrameRef, showManualModal, showAboutModal, showSaveLoadModal, setShowManualModal, setShowAboutModal, setShowSaveLoadModal}: PlaygroundProps) {
  const frameRef = useRef<HTMLIFrameElement>(null);
  const [showFps, setShowFps] = useState(false);
  const [showControl, setShowControl] = useState(false);
  const [isSplashFinished, setIsSplashFinished] = useState(false);
  const [showCanvas, setShowCanvas] = useState(false);
  const isMobile = isMobileDevice();

  useEffect(() => {
    setFrameRef(frameRef);
  }, [setFrameRef]);

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
    };

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        handleEscape();
      }
    };

    window.addEventListener('keydown', handleKeyDown);

    const handleIframeMessage = (event: MessageEvent) => {
      if (event.data && event.data.type === 'escapeKeyDown') {
        handleEscape();
      }
    };

    window.addEventListener('message', handleIframeMessage);

    const autoSave = () => {
      const currentWindow = frameRef.current?.contentWindow;
      if (currentWindow) {
        const emulatorService = CreateEmulatorService(currentWindow);
        const count = emulatorService.getSaveStatesCount();
        if (count > 0) {
          emulatorService.saveState(count - 1);
        }
      }
    };

    const handleVisibilityChange = () => {
      if (document.visibilityState === 'hidden') {
        autoSave();
      }
    };

    document.addEventListener('visibilitychange', handleVisibilityChange);

    const autoSaveInterval = setInterval(() => {
      autoSave();
    }, 30000);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('message', handleIframeMessage);
      document.removeEventListener('visibilitychange', handleVisibilityChange);
      clearInterval(autoSaveInterval);
    };
  }, [showControl, showManualModal, showAboutModal, showSaveLoadModal]);

  useEffect(() => {
    const handleIframeLoad = () => {
      const iframeWindow = frameRef.current?.contentWindow;
      if (iframeWindow) {
        (iframeWindow as any).KiwiMachineCallback = {
          onSplashFinished: () => {
            setIsSplashFinished(true);
          },
          onVolumeChanged: (data: { volume: number }) => {
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
  }, []);

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
      />

      <VirtualController 
        onButtonPress={handleButtonPress} 
        onButtonRelease={handleButtonRelease}
        onMenuButtonClick={() => setShowControl(!showControl)}
      />

      {!isMobileDevice() && <div className="playground-float-button" onClick={() => setShowControl(!showControl)}>
        <svg className="playground-float-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <circle cx="12" cy="12" r="3"></circle>
          <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06-.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
        </svg>
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
      />
    </div>
  );
}
