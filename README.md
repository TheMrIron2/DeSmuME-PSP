# DSonPSP

DSonPSP is an effort by TheMrIron2, z2442 and other PS Homebrew developers to update the proof of concept DS emulator, DSonPSP.

(Note: This project is considering a rebrand when the "desmPSP" branch becomes useful, but for now it is retaining the original code base and will stick with the DSonPSP name.)

Read below for the background information, credits and instructions.

## Background

DSonPSP was a proof of concept DS emulator for PSP by Yoshihiro. It was based on what is now ancient 2006/2007 Desmume code, and as a result it is very slow and most titles don't work very well. A few versions were released, up to beta v0.7; the master branch is based on the last publicly available source code, for 0.6.

## Programming

DSonPSP plans to use modern PSP optimisations and newer code to improve DSonPSP's compatibility and performance, in hopes of playable DS emulation.

In its current form on the master branch, DSonPSP is based on a build of Desmume that is over 10 years old. The "desmpsp" branch has updated Desmume code from the last stable release, but is significantly slower and features no PSP optimisation - and also has issues with compiling right now.

## Instructions

Create a folder (call it whatever you like, eg. "DSonPSP") in /PSP/GAME/, so the structure is as follows: /PSP/GAME/DSonPSP/.
Place the EBOOT.PBP in this folder, and within this folder create another folder called /NDSROM/. Place any DS ROMs into this folder.
Now you can simply boot DSonPSP from your PSP and the ROMs will show up automatically.

## (Developer Info) Planned Optimisations

DSonPSP is currently woefully optimised, but we have a plan to get full speed! Currently, DSonPSP is software rendered, and only uses the main CPU. Our plan:

- Remove SDL from the project, making all instructions native to the PSP.
- Offload graphics work (DS graphics core emulation) to PSP GPU
- Use both the Media Engine and main CPU and offload code to ME (Important!)^
- Use newer, more compatible and faster code
- Add an ARM to MIPS dynamic recompiler

^ The PSP has two CPUs, not just one. The main one is known to us all, but the Media Engine is actually a second CPU in the PSP. It is functionally identical to the main CPU and even runs at the same clock speed! So our plan is to use the ME to emulate the DS's secondary CPU, the 33MHz ARM7 chip. This frees up a significant amount of resources on the main CPU for us to emulate the main ARM9 chip in the DS!

So in conclusion, this is how we'll emulate the DS:
- Main PSP CPU emulates the main 67MHz ARM9 CPU
- Media Engine emulates the 33MHz ARM7 CPU (notably responsible for audio, also processes auxilary tasks like input and real time clocks)
- GPU renders graphics
- On 64MB PSPs, some games may be able to be loaded into the memory to improve loading times

We hope to get there one day!

## Credits 

My name might be on the repository, but I couldn't have done this on my own. I'll list the credits in no particular order:

- z2442: Compilation fixes
- TheMrIron2: Tweaks
- mrneo240: Help with compiling
- bandithedoge: pic1.png and icon0.png
- Exophase: ARM7 code and hardware assistance
- hcfcoder: Port of modern Desmume to PSP
- iyenal: Tweaks and fixes

## Footnote

Note that this project is a hobby for everyone involved, and due to our limited time among other real world factors we may not be able to achieve everything we set out to, including bug fixes and such. Any help is welcomed, but we are not miracle workers and our only objective is to update and improve some code.

Please share this repository with your friends! Any interest, attention or publicity will give us motivation to continue working on this and it may also find people who are interested in helping us out, as we are a very small project at the moment. 
