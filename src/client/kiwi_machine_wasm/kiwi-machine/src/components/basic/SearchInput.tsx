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

import "./SearchInput.css"
import {FormEventHandler} from "react";

interface SearchInputProps {
  text: string,
  onInput: FormEventHandler<HTMLInputElement>,
}

export default function SearchInput({text, onInput}: SearchInputProps) {
  return (
    <div className="search">
      <input type="text" className="search-input" name="" placeholder={text} onInput={onInput}/>
    </div>
  );
}