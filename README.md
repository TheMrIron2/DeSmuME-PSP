# DSonPSP

This branch of DSonPSP is a port of modern stable Desmume by HCFcoder.
It is effectively _completely_ unoptimised in its current state, but we hope to fix this and get games running respectably.

Currently, we have some preliminary performance numbers (all unoptimized) showing that on a real PSP at 333MHZ, with no GPU optimization nor a Dynamic Recompiler, the game runs software 2D (and limited 3D) at <1-3 FPS for almost all games. The optimizations need to roughly make the entire cycle 132x faster than currently. A lot of the optimizations needed can come from moving software 2D + 3D to hardware, alongside possible dynamic recompilation, media engine queueing, and more.

The current optimizations and changes planned (unordered) are as follows:
- Moving sound code to the Media Engine processor
- Moving other program tasks to the Media Engine (job system)
- Removing SDL calls and integrating native PSP calls
- Creating a hardware-accelerated 2D & 3D renderer
- Writing a new ARM9 dynarec (a big task!)
- Replacing the ARM7 interpreter with Exophase's gpSP dynarec (thanks to Exophase for permission!)
- Optimizing DSonPSP using assembly code

With new additions to the team, we should be able to continue to develop this application.
However, we would still appreciate help!
Come talk to us on our discord server at https://discord.gg/bePrj9W if you're interested in helping out!
