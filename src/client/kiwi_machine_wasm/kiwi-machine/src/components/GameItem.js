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

import "./GameItem.css"
import Button from "./basic/Button";
import {
  getROMUrlFromContents,
  getROMImageUrlFromContents,
  getLocaleTitleFromContents,
} from "../services/rom";
import Modal from "./basic/Modal";
import {useState} from "react";

export default function GameItem({contents, loadRom, romName, romId}) {
  const [detailModal, setDetailModal] = useState(false);

  const onLoadRom = function () {
    return () => {
      loadRom(getROMUrlFromContents(contents), contents.name);
    }
  }

  const gameItemClass = "gameitem " +
    (((romName === '' && romId === 0) || romName === contents.name) ? "gameitem-checked" : "");

  return (
    <div className={gameItemClass}>
      <Modal show={detailModal} title="游戏介绍">
        <div className="gameitem-flex">
          <img className="gameitem-modal-item gameitem-modal-thumbnail" src={getROMImageUrlFromContents(contents)}
               alt={contents.name}/>
          <div className="gameitem-modal-item gameitem-modal-content">
            <p>英文名：{contents.name}</p>
            <p>中文名：{contents.zh}</p>
            <p>日文名：{contents.ja}</p>
            <div style={{height: '20px'}}></div>
            <Button text='关闭' onClick={() => setDetailModal(false)}/>
          </div>
        </div>
      </Modal>
      <img src={getROMImageUrlFromContents(contents)} alt={contents.name}/>
      <div className="gameitem-contents">
        <p>{getLocaleTitleFromContents(contents)}</p>
        <Button onClick={onLoadRom()} text="开始游戏"/>
        <Button onClick={() => setDetailModal(true)} text="游戏资料"/>
      </div>
    </div>
  )
}
