# 🎶 DRAGON Music Player

A lightweight, keyboard-first terminal music player for Windows.(around 30mbs of RAM and ~1% of CPU for my laptop results may vary)

DRAGON is built with **C++20**, **FTXUI**, and **miniaudio** and is designed for playing local music files directly from the terminal.

No streaming services.  
No accounts.  
No unnecessary background services.

---

## Screenshots

### DRAGON Music Player

![DRAGON Music Player Screenshot 1](SS1.png)

### DRAGON Music Player

![DRAGON Music Player Screenshot 2](SS2.png)

---

## Features

- 🎵 Local music playback
- 📁 Recursive music folder scanning
- ▶️ Play / Pause
- ⏭️ Next song
- ⏮️ Previous song
- 💿 Animated terminal vinyl
- 📊 Animated audio visualizer
- 🎨 Multiple interface moods
- 💾 Remembers your music folder
- ⚡ Lightweight terminal application
- 📴 Local music playback

---

## Supported Audio Formats

DRAGON currently supports:

```text
.mp3, .wav, .flac, .ogg, .m4a
```

---

## Environment

```text
Operating System : Windows 10 / Windows 11
Language         : C++20
Compiler         : GCC / MinGW
Build System     : CMake
Terminal UI      : FTXUI
Audio Engine     : miniaudio
```

---

## Setup DRAGON on Windows

Follow the steps below to set up DRAGON on a Windows device.

All commands in this guide are intended to be run in **PowerShell**.

---

### 1. Install GCC / MinGW

DRAGON requires a **C++20-compatible GCC compiler**.

Install WinLibs MinGW using:

```
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

After installation, close PowerShell and open it again.

Check the GCC version:

```
gcc --version
```

Check that GCC is available in PATH:

```
where.exe gcc
```

---

### 2. Install CMake

CMake is used to configure and build DRAGON.

Install CMake:

```
winget install --id Kitware.CMake -e
```

After installation, close PowerShell and open it again.

Check the CMake version:

```
cmake --version
```

Check that CMake is available in PATH:

```
where.exe cmake
```

---

### 3. Download the DRAGON Source Code

Clone the repository using Git:

```
git clone https://github.com/sumit12c/Terminal_Music_Player.git
```

> Replace `YOUR_USERNAME` with the GitHub username that owns this repository.

Enter the project directory:

```
cd MUSIC_PLAYER_trm
```

---

### 4. FTXUI Setup

DRAGON uses **FTXUI** for its terminal user interface.

The project uses:

```
FTXUI v7.0.3
```

You do **not** need to manually download or install FTXUI.

CMake automatically downloads FTXUI when the project is configured.

---

### 5. Configure the Project

From inside the DRAGON project directory:

```
cmake -S . -B build -G "MinGW Makefiles"
```

CMake will configure the project and automatically download the required FTXUI dependency.

---

### 6. Build DRAGON

Build the project using:

```
cmake --build build --clean-first
```

After a successful build, the executable will be located at:

```
build\MusicPlayerTRM.exe
```

---

### 7. Run DRAGON

Run DRAGON from PowerShell:

```
.\build\MusicPlayerTRM.exe
```

On the first launch, DRAGON asks for your music folder.

Example:

```
Enter your music folder:

> C:\Users\YourName\Music
```

Enter the path to the folder containing your music.

DRAGON will recursively scan the folder and its subfolders for supported audio files.

---

## Music Folder

DRAGON remembers the music folder you selected.

The folder path is stored locally in:

```
config.txt
```

On future launches, DRAGON automatically uses the saved music folder.

You do not need to enter the folder again.

If you want to select a different music folder, delete `config.txt`:

```
Remove-Item .\config.txt
```

Then start DRAGON again:

```
.\build\MusicPlayerTRM.exe
```

---

## Rebuilding After Code Changes

If you modify `src/main.cpp` or another project file, rebuild using:

```
cmake --build build --clean-first
```

Then run:

```
.\build\MusicPlayerTRM.exe
```

---

## Completely Clean the Build

To remove the generated CMake build directory:

```
Remove-Item -Recurse -Force .\build
```

Configure the project again:

```
cmake -S . -B build -G "MinGW Makefiles"
```

Then build:

```
cmake --build build --clean-first
```

Run:

```
.\build\MusicPlayerTRM.exe
```

---

## Add the `dragon` Command

To launch DRAGON without typing the full executable path, add the project folder
to your Windows User PATH.

Run these commands from the DRAGON project directory:

```powershell
$dragonPath = (Get-Location).Path

[Environment]::SetEnvironmentVariable(
    "Path",
    [Environment]::GetEnvironmentVariable("Path", "User") + ";" + $dragonPath,
    "User"
)

where.exe dragon
```

Close PowerShell and open a new PowerShell window so the updated User PATH is
loaded. Then launch DRAGON from any directory with:

```powershell
dragon
```

The `dragon` command runs the [`dragon.cmd`](dragon.cmd) launcher.

---

## Lightweight Design

DRAGON is designed to remain lightweight.

The application focuses on:

- Local audio playback
- Terminal rendering
- Minimal background processing
- No streaming services
- No web interface
- No account system
- No advertisements

Actual CPU and RAM usage depends on the computer, terminal, audio file, and operating system.

---

## Local Music

DRAGON is intended for music stored on your own computer.

It does not provide music streaming.

Your music files remain in their original location.

DRAGON only scans the music folder you provide and plays supported audio files.

> **Note:** DRAGON has currently been tested only on Windows. This is an open-source project, so you are welcome to adapt it for Linux or macOS.

---

## DRAGON

```
Your music.
Your terminal.
No unnecessary complexity.
```
