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
import {FormEvent, useState} from "react";
import GameItem from "./GameItem";
import {getROMImageUrlFromContents, isLocaleTitleContains, ROMContent} from "../services/rom";
import SearchInput from "./basic/SearchInput";
import Modal from "./basic/Modal";
import Button from "./basic/Button";

interface GameListProps {
  loadRom: (romUrl: string, romName: string) => void,
  romName: string,
}

export default function GameList({loadRom, romName}: GameListProps) {
  // Set the full ROM database.
  const [gameDb, setGameDb] = useState<ROMContent[]>();

  // Current keyword for filter.
  const [keyword, setKeyword] = useState('');

  // Whether show the ROM detail modal.
  const [modalVisible, setModalVisible] = useState(false);

  // Set detail modal's content.
  const [modalContent, setModalContents] = useState<ROMContent>();

  if (!gameDb) {
    fetch('roms/db.json').then((response) => {
      response.json().then(db => {
        setGameDb(db);
      })
    })
  }

  const updateKeyword = function (event: FormEvent<HTMLInputElement>) {
    setKeyword(event.currentTarget.value)
  }

  const showDetailModal = (show: boolean, contents: ROMContent) => {
    setModalContents(contents);
    setModalVisible(true);
  }

  if (!gameDb) {
    return <div/>;
  } else {
    const db = gameDb as ROMContent[];
    const items = db.filter(item => {
      return isLocaleTitleContains(item, keyword)
    }).map(item => {
      return <GameItem key={item.id} contents={item} loadRom={loadRom} romName={romName} romId={item.id}
                       showDetailModal={showDetailModal}/>
    });

    return (
      <div>
        <Modal show={modalVisible} title="游戏介绍" setVisible={setModalVisible}>
          <div className="gamelist-flex">
            <img className="gamelist-modal-item gamelist-modal-thumbnail" loading="lazy"
                 src={getROMImageUrlFromContents(modalContent ? modalContent : null)}
                 alt={modalContent?.name}/>
            <div className="gamelist-modal-item gamelist-modal-content">
              <p>英文名：{modalContent?.name}</p>
              <p>中文名：{modalContent?.zh}</p>
              <p>日文名：{modalContent?.ja}</p>
              <div style={{height: '20px'}}></div>
              <Button text='关闭' onClick={() => setModalVisible(false)}/>
            </div>
          </div>
        </Modal>

        <div className="gamelist">
          <div className="gamelist-input">
            <SearchInput onInput={updateKeyword} text='搜索你喜欢的游戏'/>
          </div>
          <div className="gamelist-items">
            {items}
          </div>
        </div>
      </div>
    );
  }
}
