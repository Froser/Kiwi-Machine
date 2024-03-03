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

function LogoTile(key) {
  return (
    <div className='background-tile' key={key}>
      <img src='/kiwi.png' alt='Kiwi Machine'></img>
    </div>
  )
}

function LogoRow(key) {
  const arbitrary = new Array(20)
  for (let i = 0; i < arbitrary.length; ++i) {
    arbitrary[i]= (LogoTile(i))
  }

  return (
    <div className='background-row' key={key}>
      {arbitrary}
    </div>
  )
}

export default function Background() {
  const arbitrary = new Array(20)
  for (let i = 0; i < arbitrary.length; ++i) {
    arbitrary[i]= (LogoRow(i))
  }
  return (
    <div className="background">
      {arbitrary}
    </div>
  );
}
