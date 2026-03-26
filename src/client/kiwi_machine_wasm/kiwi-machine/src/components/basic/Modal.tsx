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
import {Dispatch, ReactNode, SetStateAction, useEffect, useCallback} from "react";

interface ModalProps {
  children: ReactNode,
  title: string,
  show: boolean,
  width?: string,
  height?: string,
  zIndex?: number,
  setVisible: Dispatch<SetStateAction<boolean>>,
  onClose?: () => void,
}

export default function Modal({children, title, show, width, height, zIndex, setVisible, onClose}: ModalProps) {
  const className = 'modal ' + (show ? 'modal-show' : '');
  const isAutoHeight = height === 'auto';

  const handleClose = useCallback(() => {
    setVisible(false);
    onClose?.();
  }, [setVisible, onClose]);
  
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape' && show) {
        event.preventDefault();
        event.stopPropagation();
        handleClose();
      }
    };

    document.addEventListener('keydown', handleKeyDown);
    return () => {
      document.removeEventListener('keydown', handleKeyDown);
    };
  }, [show, handleClose]);
  
  return (
    <div className={className} style={{zIndex: zIndex}} onClick={handleClose}>
      <div style={{
        width: `${width ? width : '550px'}`,
        height: isAutoHeight ? 'auto' : `${height}`,
        maxHeight: '90vh'
      }} onClick={(event) => event.stopPropagation()}>
        <div className="modal-header">
          <span>{title}</span>
          <button className="modal-close-button" onClick={handleClose} aria-label="关闭">
            <svg className="modal-close-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <line x1="18" y1="6" x2="6" y2="18"></line>
              <line x1="6" y1="6" x2="18" y2="18"></line>
            </svg>
          </button>
        </div>
        <div className="modal-contents" style={{
          height: isAutoHeight ? 'auto' : 'auto',
          overflowY: 'auto',
          flex: isAutoHeight ? '0 1 auto' : '1',
          padding: 0
        }}>
          {children}
        </div>
      </div>
    </div>
  );
}