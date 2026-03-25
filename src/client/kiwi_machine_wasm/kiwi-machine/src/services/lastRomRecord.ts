
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

const LAST_PLAYED_GAME_ID_KEY = 'lastPlayedGameId';
const LAST_PLAYED_GAME_NAME_KEY = 'lastPlayedGameName';

interface LastRomRecord {
  id: number;
  name: string;
}

function getLastRomRecord(): LastRomRecord | null {
  const id = localStorage.getItem(LAST_PLAYED_GAME_ID_KEY);
  const name = localStorage.getItem(LAST_PLAYED_GAME_NAME_KEY);
  
  if (id && name) {
    return {
      id: parseInt(id),
      name: name
    };
  }
  
  return null;
}

function setLastRomRecord(id: number, name: string): void {
  localStorage.setItem(LAST_PLAYED_GAME_ID_KEY, id.toString());
  localStorage.setItem(LAST_PLAYED_GAME_NAME_KEY, name);
}

function clearLastRomRecord(): void {
  localStorage.removeItem(LAST_PLAYED_GAME_ID_KEY);
  localStorage.removeItem(LAST_PLAYED_GAME_NAME_KEY);
}

export {
  getLastRomRecord,
  setLastRomRecord,
  clearLastRomRecord,
  type LastRomRecord
};
