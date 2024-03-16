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

import "./VolumePanel.css"
import VolumeSlider from "./basic/VolumeSlider";
import VolumeImage1 from "./resources/volume1.svg"
import VolumeImage2 from "./resources/volume2.svg"
import VolumeImage3 from "./resources/volume3.svg"
import MuteImage from "./resources/mute.svg"
import {ChangeEvent, RefObject, useEffect, useState} from "react";
import {CreateEmulatorService} from "../services/emulator";

interface VolumePanelProps {
  id: string,
  frame: RefObject<HTMLIFrameElement>
}

export default function VolumePanel({id, frame}: VolumePanelProps) {
  const threshold = 0.05;
  const [muteImage, setMuteImage] = useState(VolumeImage1);
  const [volumeCache, setVolumeCache] = useState(0);
  const [volume, setVolume] = useState(1);

  const refreshMuteImage = (volume: number) => {
    if (volume <= threshold) {
      setMuteImage(MuteImage);
    } else if (volume < .33) {
      setMuteImage(VolumeImage1);
    } else if (volume < .66) {
      setMuteImage(VolumeImage2);
    } else {
      setMuteImage(VolumeImage3);
    }
  }

  const onMuteChanged = () => {
    const currentFrame = frame.current;
    const input = window.document.getElementById(id) as HTMLInputElement;

    const emulator_service = CreateEmulatorService(currentFrame?.contentWindow);
    const volume = input.valueAsNumber;
    const muted = volume < threshold;

    if (muted) {
      emulator_service.setVolume(volumeCache);
      refreshMuteImage(volumeCache);
    } else {
      const volume_before = input.valueAsNumber;
      emulator_service.setVolume(0);
      setVolumeCache(volume_before);
      refreshMuteImage(0);
    }

    currentFrame?.contentWindow?.focus();
  }

  const onChangeInternal = (event: ChangeEvent<HTMLInputElement>) => {
    const volume = event.target.valueAsNumber;
    refreshMuteImage(volume);
    setVolume(volume);

    const currentFrame = frame.current;
    const emulator_service = CreateEmulatorService(currentFrame?.contentWindow);
    emulator_service.setVolume(event.target.valueAsNumber);
    currentFrame?.contentWindow?.focus();
  }

  useEffect(() => {
    const input = window.document.getElementById(id) as HTMLInputElement;
    const volume = input.valueAsNumber;
    setVolumeCache(volume);
    refreshMuteImage(volume);
  }, [id]);

  useEffect(() => {
    window.addEventListener('message', (event) => {
      const message = event.data;
      switch (message.type) {
        case 'volumeChanged':
          const volume = message.data.volume;
          refreshMuteImage(volume);
          setVolume(volume);
          break;
        default:
          break;
      }
    }, false);
  }, []);

  return (
    <div className='volumepanel'>
      <img className="volumepanel-mute" src={muteImage} alt="Volume" onClick={onMuteChanged}/>
      <VolumeSlider id={id} onChange={onChangeInternal} value={volume}/>
    </div>
  );
}
