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

import "./VolumeSlider.css"
import {ChangeEventHandler} from "react";

interface VolumeSliderProps {
  className?: string,
  id: string,
  onChange: ChangeEventHandler<HTMLElement>,
  value: number,
}

export default function VolumeSlider({className, id, onChange, value}: VolumeSliderProps) {
  return (
    <input
      id={id}
      className={className + " volume-slider"}
      type="range"
      min={0}
      max={1}
      step={0.1}
      value={value}
      onChange={onChange}
    />
  );
}
