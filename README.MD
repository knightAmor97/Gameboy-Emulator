# GameBoy Emulator
Game Boy emulator written in C++ (currently in development).

This is a Game Boy emulator that I have been developing over the past 4–5 months. My primary motivation was a deep curiosity about how this piece of technological history works on the inside. To scratch that itch, I dove into low-level programming, learned how individual hardware components and registers operate, and brought this project to life.

This is my first project on this scale. While my background is usually focused on Machine Learning algorithms and Procedural AI, building this emulator has given me the opportunity to explore a completely different area of Computer Science, understand how software can replicate hardware, and significantly sharpen my C++ skills.

The emulator is still under active development (roughly **70% complete**), so a full release version is not available yet. There are still features to implement, performance improvements to make, and bugs to fix!

bug reports, feedback, and pull requests will be welcomed!


## Disclaimer & ROMs usage 
*(Sorry for allformal legal speak as the only developer on this project I need to make sure expectations and legal guidelines are clear.)** 

Please ensure that any ROMs you use with this emulator are legally acquired.

* **No ROMs included:** This repository does not contain or distribute any ROM files or BIOS images
* **Legal ownership:** Please only use ROM files dumped from game cartridges you own, or homebrew/open-source software
* **User Agreement:** Users are legally responsible for any software they choose to download illegally. The developer of this emulator (me) assumes no liability for any illegal misuse of copyright material or any personal data lost.



## Download 
[Download the latest release](https://github.com/knightAmor97/Gameboy-Emulator/releases/latest)

## Feedback & bug reports
Help shape this emulator if you encounter any bugs weird visuals or have suggested changes I would love to hear them!. Please submit them below

👉 **[Form to submit a Bug report or feature Request](https://docs.google.com/forms/d/e/1FAIpQLSfZ4CGfpolM9HC7gBQvHvmIjH3pmmQURS_Mqs8GCKlRTKa7gA/viewform?usp=publish-editor)**

**(Again apologies for the formal legal talk - its just here to keep both of us protected!)**

* **Ethical Testing Note:** By submitting this form, you voluntarily agree to participate in this testing. 
* **Data Privacy:** Submissions are completely anonymous and no data will be shared with any third parties

### System Requirements
* **OS:** Windows 10/11 (Mac and Linux support Coming Soon!)

### Windows security warning:
Since I am an independent developer and this emulator is unsigned, Windows may display a *Windows protected your PC warning when first launching the emulator

To bypass this:
1. Click **More info**
2. Click **Run anyway**

This is normal behaviour for software built by unrecognizable publishers and is completely safe to run.

## How it works
At its core, this emulator works by allowing your computer hardware to mimic the architecture of a Game Boyusing Low-level Emulation (LLE). I've tried to replicate component behaviours as accurately as possible.

- **CPU:** executes gameboy opcodes, manipulates CPU registers and tracks instruction cycles.
- **MMU:** handles memory read and writes, ROM banking, cartridge RAM and hardware register access
- **PPU:** tracks LCD modes scanlines, renders window tiles and sprites
- **APU:** Models the 4 Game boy audio channels, their timers, waveform generation, envelopes, sweep and audio registers.
- **Timer:** handles internal timing tasks and interrupts for games - currently finishing

## Controller support
controller support will be added in a later version. For now, this is limited to keyboard controls

## Keyboard Controls
- up arrow - up dpad
- down arrow - down dpad
- left arrow - left dpad
- right arrow - right dpad

- z - A button
- x - B button
- Start - Enter
- Select - Right Shift

## Future Additions
When this project is finished, a **Game Boy Color Emulator** will be in the works! since the core hardware framework is built already developing GBC emulator should take significantly less time.
