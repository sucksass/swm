SWM

Static Window Manager

"SWM" (screenshot.png)

SWM is a small, fast, configurable X11 window manager.

It provides static tiling, monocle and floating layouts, fullscreen support, tags, multi-monitor support, and ICCCM/EWMH support.

SWM is written in C and built around the standard X11/Xlib interfaces. The aim is to provide a straightforward window manager that does its job without trying to become the rest of the desktop.

Features

- Static tiling
- Monocle layout
- Floating windows
- Fullscreen support
- Tags
- Multi-monitor support
- Xinerama support
- ICCCM/EWMH support
- Configurable appearance
- Keyboard and mouse bindings
- Small, standalone C implementation

Building

Clone the repository and build SWM:

git clone https://github.com/sucksass/swm
cd swm
make clean
make
make install

Configuration is handled through "config.def.h".

Change the configuration, rebuild, and install again. No configuration daemon, settings database, or extra layer between you and the source.

Just C.

Just "make".

Philosophy

SWM is built around a simple idea: a window manager should manage windows, not become the environment around them.

The implementation aims to stay small enough to understand and practical enough to modify. Features are kept focused on window management rather than accumulating functionality simply because it could be added.

That also means SWM doesn't try to solve problems that already have better standalone solutions. Your compositor can be your compositor. Your panel can be your panel. Your notification daemon can be your notification daemon.

SWM manages the windows.

The rest is your problem.

AI-Native Development

SWM is an entirely vibe-coded project.

Not AI assisted. Not written with some AI help.

AI to AI, with almost zero human interference.

The project uses two distinct AI roles.

The Descriptor AI analyzes the behavior of the reference implementation and turns what it observes into a detailed behavioral specification.

The Implementor AI receives that specification and implements SWM from it, without access to the original source code and without searching for or consulting its implementation.

In effect, the two sides are separated by a Chinese Great Wall: one AI describes what the software does, while the other figures out how to build it.

The process, methodology, and the reasoning behind this setup are documented in ""CLEANROOM.md"" (CLEANROOM.md).

This README was also written by AI.

because the human apparently has better things to do.

Documentation

- ""DOCUMENTATION.md"" (DOCUMENTATION.md) — behavior, architecture, configuration, and development
- ""CLEANROOM.md"" (CLEANROOM.md) — clean-room methodology and AI implementation workflow
- ""COMPARISON.md"" (COMPARISON.md) — technical comparisons
- ""swm.1"" (swm.1) — manual page

If the clean-room process sounds unusual, it is. Read "CLEANROOM.md".

Clarification

SWM is an independent clean-room reconstruction project.

The implementation was developed from behavioral specifications rather than by taking an existing window manager's source and modifying it.

SWM is not affiliated with, endorsed by, or derived from DWM or suckless.

Some similarities are unavoidable. X11 provides a relatively constrained set of mechanisms for managing windows, and minimalist tiling window managers naturally share concepts, terminology, and design patterns.

If you've used DWM, parts of SWM may look familiar.

That's the nature of the territory.

It's still SWM.

License

SWM is licensed under the GNU General Public License v3.0.

See ""LICENSE"" (LICENSE) for the full license text.

---

SWM — Static Window Manager

Powered by X11.
Written in C.
Vibe coded by AI.