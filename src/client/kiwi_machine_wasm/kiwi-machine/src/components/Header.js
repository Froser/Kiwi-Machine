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

import "./Header.css"

export default function Header({content}) {
  const styleList = [
    'header-color-b',
    'header-color-y',
    'header-color-r',
    'header-color-g',
    'header-color-r',
    'header-color-g',
    'header-color-y',
    'header-color-g',
  ]

  let id = 0;
  let style_id = 0;

  const spans = content.split('').map(c => {
    const className = 'header-title ' + styleList[style_id % styleList.length];
    const result = <span key={id} className={className}>{c}</span>;
    if (c !== ' ')
      style_id++;
    id++;
    return result;
  });

  return (
    <header className="header">
      <div className="header-container">
        {spans}
      </div>
    </header>
  );
}
