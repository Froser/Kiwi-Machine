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

import './LoadingSplash.css';
import { useEffect, useState } from 'react';

interface LoadingSplashProps {
  onFinished: () => void;
  isReady: boolean;
}

export default function LoadingSplash({ onFinished, isReady }: LoadingSplashProps) {
  const [isHidden, setIsHidden] = useState(false);
  const [shouldFadeOut, setShouldFadeOut] = useState(false);
  const [startTime] = useState(Date.now());

  useEffect(() => {
    if (isReady && !shouldFadeOut) {
      const elapsed = Date.now() - startTime;
      const minDuration = 2000;
      const remainingTime = Math.max(0, minDuration - elapsed);
      
      const timer = setTimeout(() => {
        setShouldFadeOut(true);
      }, remainingTime);
      
      return () => clearTimeout(timer);
    }
  }, [isReady, shouldFadeOut, startTime]);

  useEffect(() => {
    if (shouldFadeOut && !isHidden) {
      const fadeTimer = setTimeout(() => {
        setIsHidden(true);
        onFinished();
      }, 800);
      return () => clearTimeout(fadeTimer);
    }
  }, [shouldFadeOut, isHidden, onFinished]);

  return (
    <div className={`loading-splash ${isHidden ? 'hidden' : ''} ${shouldFadeOut ? 'fade-out' : ''}`}>
      <div className="glow-effect" />

      <div className="logo-container">
        <img
          src="kiwi.png"
          alt="Kiwi Machine"
          className="logo-image"
        />
        <div className="loading-text">
          Loading
          <span className="loading-dots">
            <span className="loading-dot" />
            <span className="loading-dot" />
            <span className="loading-dot" />
          </span>
        </div>
        <div className="progress-bar">
          <div className="progress-fill" />
        </div>
      </div>
    </div>
  );
}
