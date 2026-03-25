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

const PARTICLE_COUNT = 60;

export default function LoadingSplash({ onFinished, isReady }: LoadingSplashProps) {
  const [isHidden, setIsHidden] = useState(false);
  const [shouldFadeOut, setShouldFadeOut] = useState(false);
  const [particles, setParticles] = useState<Array<{ id: number; x: number; y: number; size: number; delay: number; color: string }>>([]);
  const [startTime] = useState(Date.now());

  useEffect(() => {
    const newParticles = [];
    for (let i = 0; i < PARTICLE_COUNT; i++) {
      newParticles.push({
        id: i,
        x: Math.random() * 100,
        y: Math.random() * 100,
        size: Math.random() * 4 + 2,
        delay: Math.random() * 4,
        color: `rgba(${Math.floor(Math.random() * 255)}, ${Math.floor(Math.random() * 255)}, ${Math.floor(Math.random() * 255)}, 0.8)`,
      });
    }
    setParticles(newParticles);
  }, []);

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
      <div className="particles-container">
        {particles.map((particle) => (
          <div
            key={particle.id}
            className="particle"
            style={{
              left: `${particle.x}%`,
              top: `${particle.y}%`,
              width: `${particle.size}px`,
              height: `${particle.size}px`,
              animationDelay: `${particle.delay}s`,
              background: particle.color,
            }}
          />
        ))}
      </div>

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
