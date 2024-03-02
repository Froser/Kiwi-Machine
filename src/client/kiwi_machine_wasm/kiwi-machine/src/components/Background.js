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

import "./Background.css"


function LogoTile() {
  return (
    <div className='background-tile'>
      <img src='/kiwi.png' alt='Kiwi Machine'></img>
    </div>
  )
}

function LogoRow() {
  const arbitrary = new Array(20).fill(LogoTile(), 0, 20);

  return (
    <div className='background-row'>
      {arbitrary}
    </div>
  )
}

export default function Background() {
  const arbitrary = new Array(20).fill(LogoRow(), 0, 20);

  return (
    <div className="background">
      {arbitrary}
    </div>
  );
}
