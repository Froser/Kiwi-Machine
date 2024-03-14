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
import Modal from "./basic/Modal";
import {useEffect, useRef, useState} from "react";
import NESController from "./basic/NESController";
import VolumePanel from "./VolumePanel";
import {CreateEmulatorService} from "../services/emulator";

export default function Playground({setFrameRef}) {
  const frameRef = useRef(null);
  const [manualModal, setManualModal] = useState(false);
  const [aboutModal, setAboutModal] = useState(false);

  useEffect(() => {
    setFrameRef(frameRef);
  }, [setFrameRef]);

  return (
    <div className="playground">
      <iframe className="playground-frame" ref={frameRef} src="kiwi_machine.html" title="Kiwi Machine">
      </iframe>

      <div className='playground-control'>
        <Modal childTop="200px" show={manualModal} setVisible={setManualModal} title="操作说明" height="400px"
               width="800px">
          <div className='playground-manual-title'>操作方式说明</div>
          <div className='playground-manual-content'>在游戏中，您可以通过<b>ESC</b>唤起游戏选项菜单。</div>
          <div className='playground-manual-title'>控制器布局</div>
          <NESController/>
          <div className='playground-manual-content'>键盘映射：</div>
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
                <td></td>
              </tr>
              <tr>
                <td>Start</td>
                <td>Enter</td>
                <td></td>
              </tr>
              </tbody>
            </table>
          </div>
          <div className='playground-manual-title'>游戏搜索说明</div>
          <div className='playground-manual-content'>
            <span>在右侧的搜索栏中，可以根据游戏的中文名、中文拼音、日文名、日文假名、英文名来搜索游戏。</span>
          </div>
          <div className='playground-manual-title'>功能说明</div>
          <div className='playground-manual-content'>
            <p>目前Web Kiwi
              Machine不支持持久化存档，当页面重新加载后，您的游戏记录将会消失。您可以下载客户端版本来自动存档。</p>
            <p>在开始游戏时，请注意音量，避免因音量太大而伤到耳朵。</p>
            <p>最后，祝您游戏体验愉快！</p>
          </div>

          <Button text="关闭" onClick={() => setManualModal(false)}/>
        </Modal>

        <Modal show={aboutModal} setVisible={setAboutModal} title="关于Kiwi Machine" width="650px" height="400px">
          <div className="playground-about-contents">
            <h1>关于Kiwi Machine</h1>
            <p>
              Kiwi Machine是一个跨平台的红白机模拟器，其中内置了上百个当年风靡全球的游戏，支持键盘、鼠标操作，也支持Windows、MacOS、
              Linux、iOS、Android、Web等多个平台，支持英语、简体中文和日文三种语言，提供自动存档、读档等功能。
            </p>
            <p>当前Kiwi Machine内核版本：1.1.0</p>
            <p>作者：于益偲</p>
            <h1>不同平台差异说明</h1>
            <p>Web Kiwi Machine使用的是WebAssembly + React技术，在使用行为上与客户端版有些差异：</p>
            <ul>
              <li>由于Web Kiwi Machine是纯静态页面构成，因此无法将您的存档在线保存，您一旦重新加载页面，存档信息将会消失。
              </li>
              <li>Web Kiwi Machine屏蔽了若干功能，如回到主界面能力、调整窗口大小等。</li>
              <li>为了提高加载效率，Web Kiwi Machine将引擎和游戏分开，以确保您切换游戏的时候能得到极致的速度体验。</li>
              <li><b>玩的时候，请留意音量。</b></li>
            </ul>
            <h1>客户端版本下载</h1>
            <p><a href='https://bytedance.larkoffice.com/wiki/AG1Fwd4Tji8LLgkNmuDcEMGlnBb' target='_blank'
                  rel="noreferrer">Kiwi
              Machine 1.1.0</a></p>
            <p><a href='https://bytedance.larkoffice.com/wiki/W1o1wX2eTimtOjkP4CGc16H8nef' target='_blank'
                  rel="noreferrer">Kiwi
              Machine 1.0.0</a></p>
            <h1>源码及技术</h1>
            <p>Kiwi Machine在Github上已经开源：
              <a href='https://github.com/froser/kiwi-machine'
                 target='_blank' rel='noreferrer'>https://github.com/froser/kiwi-machine</a>
            </p>
            <p>技术内幕请参考Readme、相关技术文档，或工程中的代码注释</p>
            <p>如果在使用、编码、调试过程中遇到问题，请随时联系我，或者在Github上提issue.</p>
            <p>最后：</p>
            <p>我爱飞书。祝大家玩得开心。</p>
          </div>
          <Button text="关闭" onClick={() => setAboutModal(false)}/>
        </Modal>

        <Button text="操作说明" onClick={() => setManualModal(true)}/>
        <Button text="关于Kiwi Machine" onClick={() => setAboutModal(true)}/>
        <Button text="游戏菜单 (ESC)" onClick={() => {
          CreateEmulatorService(frameRef.current.contentWindow).callMenu();
          frameRef.current.contentWindow.focus();
        }}/>
        <VolumePanel id='volume_slider' frame={frameRef}/>
        <span style={{lineHeight: '40px'}}>By 于益偲</span>
      </div>
    </div>
  );
}
