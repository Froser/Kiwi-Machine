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
import {useRef, useState} from "react";
import Header from "./Header";
import Playground from "./Playground";
import GameList from "./GameList";
import {CreateEmulatorService} from "../services/emulator";

export default function Arcade() {
  const [frameRef, setFrameRef] = useState(useRef());
  const [romName, setRomName] = useState('');

  const loadRom = (romUrl, romName) => {
    const emulator_service = CreateEmulatorService(frameRef.current.contentWindow);
    emulator_service.loadROM(romUrl);
    setRomName(romName);
  }

  return (
    <>
      <Header content="Kiwi Machine"></Header>
      <div className="arcade">
        <Playground setFrameRef={setFrameRef}/>
        <GameList loadRom={loadRom} romName={romName}/>
      </div>
    </>
  );
}
