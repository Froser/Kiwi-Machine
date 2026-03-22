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

export default function Arcade() {
  const [frameRef, setFrameRef] = useState(useRef<HTMLIFrameElement>(null));
  const [romName, setRomName] = useState('');
  const [showGameList, setShowGameList] = useState(false);
  const [showManualModal, setShowManualModal] = useState(false);
  const [showAboutModal, setShowAboutModal] = useState(false);
  const [showLandscapeTips, setShowLandscapeTips] = useState(false);
  const [hasShownTips, setHasShownTips] = useState(false);

  const loadRom = (romUrl: string, romName: string) => {
    const currentWindow = frameRef.current?.contentWindow;
    const emulator_service = CreateEmulatorService(currentWindow);
    emulator_service.loadROM(romUrl);
    setRomName(romName);
    setShowGameList(false);
  }

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

  return (
    <>
      <Header content="Kiwi Machine" onMenuClick={() => setShowGameList(true)}></Header>
      <div className="arcade">
        <Playground 
          setFrameRef={setFrameRef}
          setShowManualModal={setShowManualModal}
          setShowAboutModal={setShowAboutModal}
        />
      </div>
      <GameList 
        loadRom={loadRom} 
        romName={romName} 
        show={showGameList} 
        setShow={setShowGameList}
      />
      <Footer />
      
      <ManualModal show={showManualModal} setVisible={setShowManualModal} />
      <AboutModal show={showAboutModal} setVisible={setShowAboutModal} />
      
      <LandscapeTips 
        show={showLandscapeTips} 
        setVisible={setShowLandscapeTips}
      />
    </>
  );
}
