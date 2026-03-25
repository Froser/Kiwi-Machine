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
import {KeyboardEventHandler, useRef, useImperativeHandle, forwardRef} from "react";

interface SearchInputProps {
  text: string,
  onKeyDown?: KeyboardEventHandler<HTMLInputElement>,
  onSearch?: (keyword: string) => void,
}

export interface SearchInputRef {
  focus: () => void;
}

const SearchInput = forwardRef<SearchInputRef, SearchInputProps>(({text, onKeyDown, onSearch}, ref) => {
  const inputRef = useRef<HTMLInputElement>(null);

  useImperativeHandle(ref, () => ({
    focus: () => {
      inputRef.current?.focus();
    }
  }));

  const handleIconClick = () => {
    if (onSearch && inputRef.current) {
      onSearch(inputRef.current.value);
    }
  };

  return (
    <div className="search">
      <input ref={inputRef} type="text" className="search-input" name="" placeholder={text} onKeyDown={onKeyDown}/>
      <svg className="search-icon" viewBox="0 0 24 24" fill="currentColor" onClick={handleIconClick}>
        <path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/>
      </svg>
    </div>
  );
});

SearchInput.displayName = 'SearchInput';

export default SearchInput;