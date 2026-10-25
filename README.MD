# GameBoy Emulator
A work in progress Game Boy emulator written in C++

This is a gameboy emulator that I have been developing over the past 4-5 months. So the question is why did I do this? well I really was interested how this piece of technological history worked in the inside. So I learnt how to work with low level code and how each components and registers worked and started working on it. 

This is my first ever time working on a project on this scale as I usually focus on Machine Learning algorithms and Procedural AI. developing this emulator has given me the opportunity to explore a completely different area of Computer Science and understand how software can replicate hardware and used this opportuinity to learn low level code and sharpen my experience in C++.

This emulator is still under development and I would call it about 65% (rough estimate) of the project so it is not currently available as a downloadable executable. There are still features to implement, improvements to make and bugs to fix.

Once the beta build is up and available and if people wish to use this please submit a bug report, feedback or a pull request will be welcomed if anything is found. As I did work on this project as the main solo developer.

## Download 
(COMING SOON!!) 

## How it works
it works how any other emulator works by allowing your computer to act like a gameboy. This emulator does use LLE processes which means I have tried my best to replicate components behaviour however this is still very unfinished and I wont be able to explain fully how it works until the project is complete. However below I have a bullet point list of how I wanted the gameboy emulator to work

- CPU: executes gameboy opcodes, manipulates CPU registers and tracks instruction cycles.
- MMU: handles memory read and writes, ROM banking, cartridge RAM and hardware register access
- PPU: tracks LCD modes scanlines, renders window tiles and sprites
- Timer: Models the 4 Game boy audio channels, their timers, waveform generation, envelopes, sweep and audio registers.

## Controller support
This should be coming after a executable is published 

When will this be avalible?
Currently I just want to focus on the gameboy components rather than creating new support for a beta.

## Keyboard Controls
up arrow - up dpad
down arrow - down dpad
left arrow - left dpad
right arrow - right dpad

z - A button
x - B button
start - Enter
Select - Right Shift