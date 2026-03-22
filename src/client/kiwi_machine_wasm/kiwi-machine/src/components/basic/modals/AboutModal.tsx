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
import {Dispatch, SetStateAction} from "react";

interface AboutModalProps {
  show: boolean;
  setVisible: Dispatch<SetStateAction<boolean>>;
}

export default function AboutModal({show, setVisible}: AboutModalProps) {
  return (
    <Modal show={show} setVisible={setVisible} title="关于 Kiwi Machine" width="700px" height="auto">
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
        <Button text="关闭" onClick={() => setVisible(false)}/>
      </div>
    </Modal>
  );
}
