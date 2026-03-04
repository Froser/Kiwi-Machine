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

import "./Modal.css"
import {Dispatch, ReactNode, SetStateAction} from "react";

interface ModalProps {
  children: ReactNode,
  title: string,
  show: boolean,
  width?: string,
  height?: string,
  setVisible: Dispatch<SetStateAction<boolean>>,
}

export default function Modal({children, title, show, width, height, setVisible}: ModalProps) {
  const className = 'modal ' + (show ? 'modal-show' : '');
  const isAutoHeight = height === 'auto';
  
  return (
    <div className={className} onClick={() => setVisible(false)}>
      <div style={{
        width: `${width ? width : '550px'}`,
        height: isAutoHeight ? 'auto' : `${height}`,
        maxHeight: '90vh'
      }} onClick={(event) => event.stopPropagation()}>
        <div className="modal-header">
          <span>{title}</span>
        </div>
        <div className="modal-contents" style={{
          height: isAutoHeight ? 'auto' : 'auto',
          overflowY: 'auto',
          flex: isAutoHeight ? '0 1 auto' : '1'
        }}>
          {children}
        </div>
      </div>
    </div>
  );
}