# WORDLE LX
```
   ██                               ███   
  ████                              ██████  
 ███████                            ███████ 
 ███████                           ████████  
  ███████                         █████████
  ███████                         █████████ 
  ███████       ████             ██████████ 
  ███████      █████           ██████████  
  ███████     ████████         █████████   
   ██████    █████████        ██████████   
   ███████   ██████████      ██████████    
   ███████  ███████████      █████████     
    ██████  ████████████    █████████      
    ██████ █████████████    █████████      
    █████████████████████  █████████       
    █████████████████████ ██████████       
     ██████████████████████████████        
     █████████████ █████                   
     █████████████  ████   ██     ██   ██  
      ███████████   ████  ██     ██   ██   
      ███████████    ███  ██      ██ ██    
      ██████████     ███  ██       ███     
       █████████      ██  ██       ███     
       ████████       ██ ██      ██ ██     
         █████           ██      ██ ██     
         ████            █████  ██   ██    
                         █████  ██   ██    
```

WORDLE LX is a Wordle-style game built for classic HP 200LX environments. The project targets both plain DOS and the HP 200LX System Manager EXM format, with shared game logic in C and separate front ends for text-mode DOS and the EXM shell integration.

This project was created using Claude Code and is set up to build with OpenWatcom, Python 3, and DOSBox-X. The repository includes generators for the word data and icon assets, plus build paths for both the DOS executable and the EXM package.

## Project Layout

- `src/` contains the shared game logic and the DOS text-mode front end.
- `exm/` contains the HP 200LX EXM-specific entry points, display code, and packaging flow.
- `make_dat.py` generates `WORDLELX.DAT`.
- `make_icon.py` generates the icon files used by the build.
- `output/` contains the final build artifacts.

## Prerequisites

- Python 3
- OpenWatcom v2
- DOSBox-X — the makefiles default to the macOS app path (`/Applications/DOSBox-X.app/Contents/MacOS/dosbox-x`); set the `DOSBOX` make variable to your executable path on other platforms
- The EXM SDK repository checked out at `../EXM`, or `EXM_ROOT` set explicitly
 - EXM SDK: https://github.com/200lx/EXM

Supported host environments:

- macOS
- Linux
- Windows via Git Bash or WSL

The current build files assume a POSIX-style shell environment. That means macOS and Linux work naturally, and Windows works best through Git Bash or WSL. Native `cmd.exe` or PowerShell builds are not first-class in the current repo.

OpenWatcom is expected to be available through the included environment script:

```sh
. ./env-watcom.sh
```

By default, that script uses `$HOME/watcom-src/rel` as `WATCOM`. If your OpenWatcom installation lives elsewhere, set `WATCOM` first and then source the script:

```sh
export WATCOM=/path/to/watcom/rel
. ./env-watcom.sh
```

## Build Artifacts

The main outputs are written to `output/`:

- `WORDLDOS.EXE` for the DOS build
- `WORDLELX.EXM` for the HP 200LX System Manager build
- `WORDLELX.DAT` word list data
- `WORDLELX.ICN` and `WORDLDOS.ICN` generated icon files

## Build From VS Code

This workspace includes tasks for the common build steps:

- `generate: word data (WORDLELX.DAT)`
- `generate: icon (WORDLELX.ICN)`
- `build: exe (WORDLDOS.EXE)`
- `build: exm package (WORDLELX.EXM)`
- `Full Build`

Running `Full Build` will generate the data and icons, build the DOS executable, and package the EXM target.

## Build From The Command Line

The commands below work in a POSIX shell. On Windows, run them from Git Bash or WSL.

### 1. Generate assets

```sh
python3 make_dat.py output/WORDLELX.DAT
python3 make_icon.py output/WORDLELX.ICN
```

The icon generator writes both `WORDLELX.ICN` and `WORDLDOS.ICN` into `output/`.

### Optional: regenerate the i812 EXM bitmap font asset

The EXM renderer can use glyphs extracted from `i812.com` (HP 200LX font loader).
The repository checks in generated C assets in `exm/i812_font_data.c` and
`exm/i812_font_data.h`.

To regenerate those files from a new or updated `i812.com`:

```sh
python3 extract_i812_font.py i812.com exm/i812_font_data.h exm/i812_font_data.c
```

This is a one-time/manual step and is not part of `wmake` by default.

### 2. Build the DOS executable

```sh
. ./env-watcom.sh
wmake
```

This produces `output/WORDLDOS.EXE`.

### 3. Build the EXM package

```sh
. ./env-watcom.sh
cd exm
wmake -f Makefile.wmake exm
```

This compiles the EXM-specific executable, converts the linker map for E2M compatibility, runs the EXM packaging step in DOSBox-X, and writes `output/WORDLELX.EXM`.

## Platform Notes

### macOS

The existing workflow should work as documented once OpenWatcom, Python 3, DOSBox-X, and the EXM SDK are installed.

### Linux

Linux can use the same commands as macOS as long as:

- `dosbox-x` is on your `PATH`
- `python3` is on your `PATH`
- `WATCOM` points to an OpenWatcom tree that includes the host tools and DOS libraries

No additional repository files are required for Linux.

### Windows

Windows can build with the current repository if you use Git Bash or WSL and have the same tools available there. In that setup, no extra repository files are strictly required.

For native Windows builds from `cmd.exe` or PowerShell, the current repo is not quite enough by itself because the makefiles and helper script assume POSIX shell commands such as `mkdir -p`, `cp`, `rm`, and shell-style environment export. For first-class native Windows support, you would likely want to add one or more of these:

- a `env-watcom.cmd` or PowerShell equivalent
- Windows-specific task definitions
- either a Windows-oriented makefile variant or further makefile portability work

So the short answer is:

- Linux: current setup works with the documented environment
- Windows with Git Bash or WSL: current setup should work
- Native Windows shell support: likely needs additional repo files or build-script changes

## Running

To run the DOS build through the top-level makefile:

```sh
. ./env-watcom.sh
wmake run
```

To launch the EXM build flow for interactive HP 200LX testing:

```sh
. ./env-watcom.sh
cd exm
wmake -f Makefile.wmake run
```

### EXM Controls

When running the EXM version on the HP 200LX System Manager:

- **F1** — New Game
- **F5** — Toggle Font (switch between original large board font and i812 custom bitmap font)
- **F8** — About
- **F9** — Help
- **F10** — Quit

The **Font toggle (F5)** lets you switch between two rendering modes:
- **Original font** (default): Uses the previous COUGRAPH large tile font (`FONT_LARGE`, 16x12) for board letters and the standard UI label font for panel text
- **i812 font**: Uses glyphs extracted from `i812.com`, giving the UI and board a custom styled appearance with the i812 bitmap font

The choice persists for the current game session only; it resets to the original font when the app restarts.

## DOS Font Note

The DOS executable (`WORDLDOS.EXE`) uses 80x25 text mode and therefore depends
on the currently active system font in video text mode. It does not blit custom
bitmap glyphs per character in text mode.

If you want DOS text to appear in the i812 style, load that font in the DOS
session first (for example by running `i812.com` before launching the game).
