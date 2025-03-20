# chip8-emu
Emulator for CHIP-8 written in C (WIP)\
Currently compatible with most Linux terminals supporting termios.\
Draws frames entirely in terminal using block characters. \
Supports most roms for classic CHIP-8 at this moment.
Input is done by using the terminal in non canonical mode. 

## Build
Use the makefile and make

## Run 
./chip8 <ROMFILE.rom>

## Additional Info
For sound, make sure the terminal you are using supports alarms ('\a' char) as sound and not flash. In my case, I had to enable the setting in xfceterm.
Input keys are a bit janky for now so make sure to hold them to properly input. The keyboard states reset every frame. Fix to come...

## TODO:
- [x] Draw IBM Logo test
- [x] Pass instruction tests
- [x] Pass Flag tests
- [x] Pass Beep test
- [x] Pass Keyboard test
- [x] Fix instruction count per frame
- [ ] Fix input lag/timing

