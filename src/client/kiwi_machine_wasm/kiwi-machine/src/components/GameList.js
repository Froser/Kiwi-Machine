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

import "./GameList.css"
import {useState} from "react";
import GameItem from "./GameItem";
import {isLocaleTitleContains} from "../services/rom";
import SearchInput from "./basic/SearchInput";

export default function GameList({loadRom, romName}) {
  const [gameDb, setGameDb] = useState(null);
  const [keyword, setKeyword] = useState('');

  if (!gameDb) {
    fetch('roms/db.json').then((response) => {
      response.json().then(db => {
        setGameDb(db);
      })
    })
  }

  const updateKeyword = function (event) {
    setKeyword(event.currentTarget.value)
  }

  if (!gameDb) {
    return <div/>;
  } else {
    const items = gameDb.filter(item => {
      return isLocaleTitleContains(item, keyword)
    }).map(item => {
      return <GameItem key={item.id} contents={item} loadRom={loadRom} romName={romName} romId={item.id}/>
    });

    return (
      <div>
        <div className="gamelist">
          <div className="gamelist-input">
            <SearchInput type='text' onInput={updateKeyword} text='搜索你喜欢的游戏'/>
          </div>
          <div className="gamelist-items">
            {items}
          </div>
        </div>
      </div>
    );
  }
}
