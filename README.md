# DSonPSP

DSonPSP v0.8 is an effort by TheMrIron2 and z2442 to update the proof of concept DS emulator, DSonPSP.

## Background

DSonPSP was a proof of concept DS emulator for PSP by Yoshihiro. It was based on what is now ancient 2006/2007 Desmume code, and as a result it is very slow and most titles don't work very well. A few versions were released, up to beta v0.7; this repository is based on the last available source code, for 0.6.

## Updating

DSonPSP v0.8 plans to use modern PSP SDK optimisations and newer code to improve DSonPSP on a compatibility level and a performance level, in hopes of getting some games to be playable.

However, there are some obstacles. DSonPSP is based on an extremely old Desmume build which dates to when the emulator was coded in C, whereas it is now in C++.

Possible solutions:

- Attempt to find last working C build and update DSonPSP for a quick fix
- Try to use a new C++ version of Desmume, discarding the old C base
- Try a whole new branch of DS emulator, such as MelonDS; this may be favourable since Desmume still has issues such as the refusal to fix bugs in Pokemon.

Microphone support among other peripherals are likely to never get added unless this becomes a project on a larger scale. This is simply focused on improving the base code; we do not have the capacity or interest to add functionality from peripherals like the microphone and instead are looking to improve compatibility and performance. 

Note that this project is a hobby for everyone involved, and due to our limited time among other factors we may not be able to achieve everything we set out to, including bug fixes and such. Any help is welcomed, but we are not miracle workers and our only objective is to update and improve some code.

Please share this repository with your friends! Any interest, attention or publicity will give us motivation to continue working on this and it may also find people who are interested in helping us out, as we are a very small project at the moment. 
