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

import "./Toast.css"
import {Dispatch, SetStateAction, useEffect, useState} from "react";

interface ToastProps {
  show: boolean,
  message: string,
  setVisible: Dispatch<SetStateAction<boolean>>,
  duration?: number
}

export default function Toast({show, message, setVisible, duration = 2000}: ToastProps) {
  const [visible, setLocalVisible] = useState(false);

  useEffect(() => {
    if (show) {
      setLocalVisible(true);
      const timer = setTimeout(() => {
        setLocalVisible(false);
        setVisible(false);
      }, duration);
      return () => clearTimeout(timer);
    }
  }, [show, setVisible, duration]);

  return (
    <div className={`toast ${visible ? 'toast-show' : ''}`}>
      <div className="toast-content">
        <span className="toast-text">{message}</span>
      </div>
    </div>
  );
}
