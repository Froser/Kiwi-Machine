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

import Modal from "../Modal";
import Button from "../Button";
import NESController from "../NESController";
import {Dispatch, SetStateAction} from "react";

interface ManualModalProps {
  show: boolean;
  setVisible: Dispatch<SetStateAction<boolean>>;
  onClose?: () => void;
}

export default function ManualModal({show, setVisible, onClose}: ManualModalProps) {
  const handleClose = () => {
    setVisible(false);
    onClose?.();
  };
  
  return (
    <Modal show={show} setVisible={setVisible} title="操作说明" height="auto" width="900px" onClose={onClose}>
      <div className="playground-manual-section">
        <div className='playground-manual-title'>操作方式说明</div>
        <div className='playground-manual-content'>
          <p>在游戏中，您可以通过按下 <b>ESC</b> 键来唤起游戏选项菜单，进行存档、读档、设置等操作。</p>
        </div>
      </div>

      <div className="playground-manual-section">
        <div className='playground-manual-title'>控制器布局</div>
        <div className="playground-manual-controller">
          <NESController/>
        </div>
      </div>

      <div className="playground-manual-section">
        <div className='playground-manual-title'>键盘映射</div>
        <div className="playground-manual-table">
          <table>
            <thead>
            <tr>
              <th>手柄按键</th>
              <th>玩家一键盘布局</th>
              <th>玩家二键盘布局</th>
            </tr>
            </thead>
            <tbody>
            <tr>
              <td>↑</td>
              <td>W</td>
              <td>↑</td>
            </tr>
            <tr>
              <td>↓</td>
              <td>S</td>
              <td>↓</td>
            </tr>
            <tr>
              <td>←</td>
              <td>A</td>
              <td>←</td>
            </tr>
            <tr>
              <td>→</td>
              <td>D</td>
              <td>→</td>
            </tr>
            <tr>
              <td>A</td>
              <td>J</td>
              <td>Del</td>
            </tr>
            <tr>
              <td>B</td>
              <td>K</td>
              <td>Ins</td>
            </tr>
            <tr>
              <td>Select</td>
              <td>L</td>
              <td>-</td>
            </tr>
            <tr>
              <td>Start</td>
              <td>Enter</td>
              <td>-</td>
            </tr>
            </tbody>
          </table>
        </div>
      </div>

      <div className="playground-manual-section">
        <div className='playground-manual-title'>游戏搜索说明</div>
        <div className='playground-manual-content'>
          <p>在右侧的搜索栏中，您可以根据以下信息快速搜索游戏：</p>
          <ul style={{margin: '8px 0 0 24px', padding: 0}}>
            <li style={{margin: '4px 0'}}>中文名</li>
            <li style={{margin: '4px 0'}}>中文拼音</li>
            <li style={{margin: '4px 0'}}>日文名</li>
            <li style={{margin: '4px 0'}}>日文假名</li>
            <li style={{margin: '4px 0'}}>英文名</li>
          </ul>
        </div>
      </div>

      <div className="playground-manual-section">
        <div className='playground-manual-title'>功能说明</div>
        <div className='playground-manual-warning'>
          <span className="playground-manual-warning-icon">!</span>
          <span><b>注意：</b>目前 Web 版 Kiwi Machine 不支持持久化存档，当页面重新加载后，您的游戏记录将会消失。如需自动存档功能，请下载客户端版本。</span>
        </div>
        <div className='playground-manual-warning'>
          <span className="playground-manual-warning-icon">🔊</span>
          <span><b>温馨提示：</b>在开始游戏时，请注意音量，避免因音量太大而伤到耳朵。</span>
        </div>
        <div className='playground-manual-success'>
          <span style={{fontSize: '20px', marginRight: '8px'}}>🎮</span>
          <span><b>祝您游戏体验愉快！</b></span>
        </div>
      </div>

      <div className="playground-manual-footer">
        <Button text="关闭" onClick={handleClose}/>
      </div>
    </Modal>
  );
}
