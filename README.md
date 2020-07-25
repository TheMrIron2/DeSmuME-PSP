# DeSmuME PSP

DeSmuME PSP is a port of modern stable DeSmuME based on the initial port by HCFcoder.
It is _completely_ unoptimised in its current state, but we hope to fix this and get games running respectably.

The plan, as described in the [2020 PSP Homebrew Dev Conference](https://youtu.be/VyHD5Hx1SYY?t=11768), is to use a dynamic rebalancing system to emulate both the ARM9 and ARM7 processors on the main CPU and Media Engine, depending on which CPU has more resources available at any given point. It is hoped that with both the main CPU and ME working in tandem, many games will run well.

Currently, the port is very basic. Everything is done on the main CPU (including all graphics and rendering) so it is very slow, and SDL is used as an abstraction layer for many parts (eg. input) rather than native PSP calls. This will hopefully change in the near future.

Any questions, or want to talk to the devs? Come and talk to us on our [discord server](https://discord.gg/bePrj9W) if you're interested!
