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

import "./Arcade.css"
import {useRef, useState, useEffect} from "react";
import Header from "./Header";
import Playground from "./Playground";
import GameList from "./GameList";
import {CreateEmulatorService} from "../services/emulator";
import Footer from "./Footer";
import ManualModal from "./basic/modals/ManualModal";
import AboutModal from "./basic/modals/AboutModal";
import LandscapeTips from "./basic/LandscapeTips";
import Toast from "./basic/Toast";
import SaveLoadModal from "./SaveLoadModal";
import {getROMUrlFromContents, ROMContent} from "../services/rom";
import {getLastRomRecord, setLastRomRecord} from "../services/lastRomRecord";

export default function Arcade() {
  const [frameRef, setFrameRef] = useState(useRef<HTMLIFrameElement>(null));
  const [romName, setRomName] = useState('');
  const [showGameList, setShowGameList] = useState(false);
  const [showManualModal, setShowManualModal] = useState(false);
  const [showAboutModal, setShowAboutModal] = useState(false);
  const [showSaveLoadModal, setShowSaveLoadModal] = useState(false);
  const [showLandscapeTips, setShowLandscapeTips] = useState(false);
  const [showToast, setShowToast] = useState(false);
  const [toastMessage, setToastMessage] = useState('');
  const [hasShownTips, setHasShownTips] = useState(false);
  const [gameDb, setGameDb] = useState<ROMContent[]>();
  const [emulatorReady, setEmulatorReady] = useState(false);
  const [hasAutoLoadedGame, setHasAutoLoadedGame] = useState(false);

  const focusIframe = () => {
    setTimeout(() => {
      frameRef.current?.focus();
    }, 100);
  };

  const loadRom = (romUrl: string, romName: string, romId: number) => {
    const currentWindow = frameRef.current?.contentWindow;
    const emulator_service = CreateEmulatorService(currentWindow);
    emulator_service.loadROM(romUrl);
    setRomName(romName);
    setShowGameList(false);
    
    setLastRomRecord(romId, romName);
    focusIframe();
  }

  useEffect(() => {
    fetch('roms/db.json').then((response) => {
      response.json().then(db => {
        setGameDb(db);
      })
    })
  }, []);

  const handleEmulatorReady = () => {
    setEmulatorReady(true);
  };

  useEffect(() => {
    if (emulatorReady && gameDb && gameDb.length > 0 && !hasAutoLoadedGame) {
      const lastRomRecord = getLastRomRecord();
      let gameToLoad = gameDb[0];
      
      if (lastRomRecord) {
        const foundGame = gameDb.find(game => game.id === lastRomRecord.id);
        if (foundGame) {
          gameToLoad = foundGame;
        }
      }
      
      const currentWindow = frameRef.current?.contentWindow;
      if (currentWindow) {
        const emulator_service = CreateEmulatorService(currentWindow);
        emulator_service.loadROM(getROMUrlFromContents(gameToLoad));
        setRomName(gameToLoad.name);
        
        setLastRomRecord(gameToLoad.id, gameToLoad.name);
        
        setHasAutoLoadedGame(true);
      }
    }
  }, [emulatorReady, gameDb, hasAutoLoadedGame]);

  useEffect(() => {
    const checkOrientation = () => {
      const isLandscape = window.innerWidth > window.innerHeight;
      if (isLandscape && !hasShownTips && !showLandscapeTips) {
        setShowLandscapeTips(true);
        setHasShownTips(true);
      }
    };

    checkOrientation();
    window.addEventListener('resize', checkOrientation);
    window.addEventListener('orientationchange', checkOrientation);

    return () => {
      window.removeEventListener('resize', checkOrientation);
      window.removeEventListener('orientationchange', checkOrientation);
    };
  }, [showLandscapeTips, hasShownTips]);

  const handleEmulatorReadyWithFocus = () => {
    handleEmulatorReady();
    focusIframe();
  };

  return (
    <>
      <Header content="Kiwi Machine" onMenuClick={() => setShowGameList(true)}></Header>
      <div className="arcade">
        <Playground 
          setFrameRef={setFrameRef}
          showManualModal={showManualModal}
          showAboutModal={showAboutModal}
          showSaveLoadModal={showSaveLoadModal}
          setShowManualModal={setShowManualModal}
          setShowAboutModal={setShowAboutModal}
          setShowSaveLoadModal={setShowSaveLoadModal}
          showToast={showToast}
          setShowToast={setShowToast}
          toastMessage={toastMessage}
          setToastMessage={setToastMessage}
          onEmulatorReady={handleEmulatorReadyWithFocus}
        />
      </div>
      <GameList 
        loadRom={loadRom} 
        romName={romName} 
        show={showGameList} 
        setShow={setShowGameList}
        onClose={focusIframe}
      />
      <Footer />
      
      <ManualModal show={showManualModal} setVisible={setShowManualModal} onClose={focusIframe} />
      <AboutModal show={showAboutModal} setVisible={setShowAboutModal} onClose={focusIframe} />
      <SaveLoadModal 
        show={showSaveLoadModal} 
        setVisible={setShowSaveLoadModal} 
        frameRef={frameRef}
        onClose={focusIframe}
      />
      
      <LandscapeTips 
        show={showLandscapeTips} 
        setVisible={setShowLandscapeTips}
      />
      
      <Toast 
        show={showToast} 
        message={toastMessage} 
        setVisible={setShowToast}
      />
    </>
  );
}
