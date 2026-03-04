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
import Button from "./basic/Button";
import Checkbox from "./basic/Checkbox";
import Modal from "./basic/Modal";
import {Dispatch, RefObject, SetStateAction, useEffect, useRef, useState} from "react";
import NESController from "./basic/NESController";
import VolumePanel from "./VolumePanel";
import {CreateEmulatorService} from "../services/emulator";

interface PlaygroundProps {
  setFrameRef: Dispatch<SetStateAction<RefObject<HTMLIFrameElement>>>
}

export default function Playground({setFrameRef}: PlaygroundProps) {
  const frameRef = useRef<HTMLIFrameElement>(null);
  const [manualModal, setManualModal] = useState(false);
  const [aboutModal, setAboutModal] = useState(false);
  const [showFps, setShowFps] = useState(false);

  useEffect(() => {
    setFrameRef(frameRef);
  }, [setFrameRef]);

  const handleShowFpsChange = (checked: boolean) => {
    setShowFps(checked);
    const currentWindow = frameRef.current?.contentWindow;
    if (currentWindow) {
      currentWindow.postMessage({
        type: 'toggleFps',
        data: { show: checked }
      }, '*');
    }
  };

  return (
    <div className="playground">
      <iframe className="playground-frame" ref={frameRef} src="kiwi_machine.html" title="Kiwi Machine">
      </iframe>

      <div className='playground-control'>
        <Modal show={manualModal} setVisible={setManualModal} title="操作说明" height="auto" width="900px">
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
            <Button text="关闭" onClick={() => setManualModal(false)}/>
          </div>
        </Modal>

        <Modal show={aboutModal} setVisible={setAboutModal} title="关于 Kiwi Machine" width="700px" height="auto">
          <div className="playground-about-contents">
            <h1>关于 Kiwi Machine</h1>
            <p>
              Kiwi Machine 是一个跨平台的红白机模拟器，内置了上百个当年风靡全球的经典游戏。
              支持键盘、鼠标操作，兼容 Windows、MacOS、Linux、iOS、Android 和 Web 等多个平台。
              提供英语、简体中文和日文三种语言，以及自动存档、读档等便捷功能。
            </p>
            <p><b>当前 Kiwi Machine 内核版本：</b>2.0.0</p>

            <h1>不同平台差异说明</h1>
            <p>Web 版 Kiwi Machine 使用 WebAssembly + React 技术，在使用体验上与客户端版有一些差异：</p>
            <ul>
              <li>由于 Web Kiwi Machine 是纯静态页面构成，因此无法将您的存档在线保存。一旦重新加载页面，存档信息将会消失。</li>
              <li>Web Kiwi Machine 屏蔽了若干功能，如回到主界面能力、调整窗口大小等。</li>
              <li>为了提高加载效率，Web Kiwi Machine 将引擎和游戏分开，以确保您切换游戏的时候能得到极致的速度体验。</li>
              <li><b>玩的时候，请留意音量。</b></li>
            </ul>

            <h1>源码及技术</h1>
            <p>Kiwi Machine 在 GitHub 上已经开源：
              <a href='https://github.com/froser/kiwi-machine'
                 target='_blank' rel='noreferrer'>https://github.com/froser/kiwi-machine</a>
            </p>
            <p>技术内幕请参考 Readme、相关技术文档，或工程中的代码注释。</p>
            <p>如果在使用、编码、调试过程中遇到问题，请随时联系我，或者在 GitHub 上提 issue。</p>
            <p style={{marginTop: '24px', fontSize: '16px', fontStyle: 'italic', color: 'var(--accent-primary)'}}>
              我爱飞书。祝大家玩得开心。
            </p>
          </div>
          <div className="playground-manual-footer">
            <Button text="关闭" onClick={() => setAboutModal(false)}/>
          </div>
        </Modal>

        <Button text="游戏菜单 (ESC)" onClick={() => {
          const currentWindow = frameRef.current?.contentWindow;
          CreateEmulatorService(currentWindow).callMenu();
          currentWindow?.focus();
        }}/>
        <VolumePanel id='volume_slider' frame={frameRef}/>
        <span className="playground-separator">&nbsp;</span>
        <span className="playground-separator">&nbsp;</span>
        <Button text="操作说明" onClick={() => setManualModal(true)}/>
        <Button text="关于Kiwi Machine" onClick={() => setAboutModal(true)}/>
        <Checkbox 
          id="showFpsCheckbox"
          label="显示帧率" 
          checked={showFps} 
          onChange={handleShowFpsChange}
        />
      </div>
    </div>
  );
}