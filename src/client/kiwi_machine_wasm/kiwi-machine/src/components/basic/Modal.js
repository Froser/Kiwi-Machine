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
import {useState} from "react";

const headerHeight = 48;

export default function Modal({children, title, show, width, height, setVisible}) {
  const className = 'modal ' + (show ? 'modal-show' : '');
  return (
    <div className={className} onClick={()=>setVisible(false)}>
      <div style={{
        width: `${width ? width : '550px'}`,
        height: `${height ? `${parseInt(height) + headerHeight}px` : '270px'}`
      }} onClick={(event) => event.stopPropagation()}>
        <div className="modal-header"
             style={{height: `${headerHeight.toString()}px`, lineHeight: `${headerHeight.toString()}px`}}>
          <span> {title}</span>
        </div>
        <div className="modal-contents" style={{height: `${height !== '' ? height : '270px'}`}}>
          {children}
        </div>
      </div>
    </div>
  );
}
