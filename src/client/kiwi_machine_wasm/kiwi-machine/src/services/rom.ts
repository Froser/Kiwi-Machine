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


export interface ROMContent {
  dir: string
  zh: string,
  ja: string,
  name: string,
  key: string,
  id: number,
}

function getROMUrlFromContents(contents: ROMContent) {
  if (contents.dir) {
    return encodeURIComponent('roms/' + contents['dir'] + '/' + contents['name'] + '.nes');
  } else {
    return encodeURIComponent('roms/' + contents['name'] + '/' + contents['name'] + '.nes');
  }
}

function getROMImageUrlFromContents(contents: ROMContent | null) {
  if (!contents)
    return '';

  if (contents.dir) {
    return 'roms/' + contents['dir'] + '/' + contents['name'] + '.jpg';
  } else {
    return 'roms/' + contents['name'] + '/' + contents['name'] + '.jpg';
  }
}

function getLocaleTitleFromContents(contents: ROMContent) {
  if (navigator.language === 'zh' || navigator.language.startsWith('zh-'))
    return contents.zh;

  if (navigator.language === 'ja' || navigator.language.startsWith('ja-'))
    return contents.ja;

  return contents.name;
}

function isLocaleTitleContains(contents: ROMContent, keyword: string) {
  if (keyword.trim() === '')
    return true;

  for (const key in contents) {
    if (key === 'id')
      continue;

    // @ts-ignore
    if (contents[key].toLowerCase().includes(keyword.toLowerCase()))
      return true;
  }

  return false;
}

export {
  getROMUrlFromContents,
  getROMImageUrlFromContents,
  getLocaleTitleFromContents,
  isLocaleTitleContains,
}