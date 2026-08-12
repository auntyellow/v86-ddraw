# ddraw-wrapper

`ddraw-wrapper` is a lightweight DirectDraw interceptor that forwards DirectDraw calls to the real `ddraw.dll` from the Windows system directory. It is useful for debugging DirectDraw usage, tracing API calls, and modifying or replacing DirectDraw behavior in legacy Windows games.

## Usage

1. Build `ddraw.dll`.
2. Copy the resulting `ddraw.dll` into the same folder as the target application.
3. Run the application. The wrapper will load the real system `ddraw.dll` and forward calls automatically.

### How to Build

- Install MSVC 6.0 (Visual Studio 6.0) or mingw32 (`sudo apt install gcc-mingw-w64-i686`)
- Run `makefile.bat` or `makefile.sh`
- To enable logging (DebugView in Windows NT or `ddraw_wrapper.log` in Windows 9x), remove the `NDEBUG` and `RA95_FLIP` macros in `makefile.bat` or `makefile.sh`.

### Build for C&C Red Alert (RA95)

- With **[VMDisp9x](https://github.com/JHRobotics/vmdisp9x)**: the wrapper is not strictly necessary to run RA95. In the [v86 emulator](https://github.com/copy/v86), VMDisp9x works well for [RA95Demo](https://www.dosgamesarchive.com/file/command-and-conquer-red-alert/ra95demo), but it can be unstable for [FunkyFr3sh's build](https://funkyfr3sh.cnc-comm.com/).
- With **[VBEMP 9x](https://bearwindows.zcm.com.au/vbe9x.htm)**: changing the macro from `RA95_FLIP` to `RA95_SKIP_CHECK` can speed up rendering. This works perfectly for FunkyFr3sh's build.
- With **[VBEMP NT](https://bearwindows.zcm.com.au/vbemp.htm)** or in Windows XP: keep the `RA95_FLIP` macro (smoother than `RA95_BLTFAST`). This also works great for FunkyFr3sh's build.

## Technical Background: How Native `ddraw.dll` Works

2D games typically interact with DirectDraw using one of two methods:

1. **Lock/Unlock**: Lock the primary surface, render to it directly, and then Unlock.
2. **Back-Buffer**: Render to a back-buffer surface, then Flip or Blt (BltFast) to push it to the primary surface.

In Windows 95/98/ME, VMDisp9x and VBEMP 9x support method 1. In Windows NT (2000, XP), VBEMP NT and the native XP VGA driver **do not** support method 1. Consequently, games relying solely on method 1 fail to run on Windows NT environments, whereas method 2 games work fine.

| Display Driver | VRAM on Primary | Lock/Unlock Primary | Flip to Primary | Blt (BltFast) to Primary |
|-|-|-|-|-|
| VMDisp9x | ✅ | ✅ | ✅ | ✅ |
| VBEMP 9x | ❌ | ✅ | ✅ | ✅ | 
| VBEMP NT | ❌ | ❌ | ✅ | ✅ |
| Windows XP \* | ❌ | ❌ | ✅ | ✅ |

\* Note: VBEMP is not required in Windows XP and later.

### Game Case Studies

**[StarCraft](https://www.chess-wizard.com/minigames/minigame_scdemo.htm)** tries to Lock the primary surface first (Method 1). If it fails, it falls back to rendering to a back-buffer and using Blt to the primary surface (Method 2). Thus, it has built-in compatibility.

**[RA95](https://www.chess-wizard.com/minigames/minigame_ra95demo.htm)** is trickier. It fails to launch if the primary surface lacks video memory page (see [Source Code](https://github.com/electronicarts/CnC_Red_Alert/blob/main/CODE/STARTUP.CPP#L485) for details). While the wrapper can spoof this check to let the game start, RA95 will become unresponsive (freeze) if it subsequently fails to lock the primary surface.

| Game | Lock/Unlock Primary | Check VRAM on Primary | Fallback to Blt |
|-|-|-|-|
| **RA95** | ✅ | ✅ | ❌ |
| **StarCraft** | ✅ | ❌ | ✅ |

### How ddraw-wrapper Solves This

The wrapper helps bypass the primary surface memory check and wraps the Lock/Unlock calls on the primary surface, converting them into Flip or BltFast operations on the fly.

| Game | VMPDisp9x | VBEMP 9x | VBEMP 9x + wrapper | VBEMP NT or WinXP | VBEMP NT or WinXP + wrapper |
|-|-|-|-|-|-|
| **RA95** | ✅<sup> [1]</sup> | ❌<sup> [2]</sup> | ✅<sup> [3]</sup> | ❌<sup> [2]</sup> | ✅<sup> [4]</sup> |
| **StarCraft** | ✅ | ✅ | ❌<sup> [5]</sup> | ✅<sup> [6]</sup> | ✅<sup> [7]</sup> |

Notes:

1. Unstable for FunkyFr3sh's build in v86.
2. Causes a fatal alert.
3. Requires the `RA95_SKIP_CHECK` macro (`RA95_FLIP` and `RA95_BLTFAST` will be slower). Placing `ddraw.dll` into the `ra95.exe` directory usually fails because Windows 98's `dsound.dll` relies on the `VidMem*` functions exported by `ddraw.dll`. As a workaround, rename `ddraw.dll` to `ddwrp.dll` and modify the Import Directory of `ra95.exe` using [CFF Explorer](https://ntcore.com/explorer-suite/).
4. Requires the `RA95_FLIP` macro (smoother than `RA95_BLTFAST`).
5. `StarCraft.exe` loads `ddraw.dll` dynamically, therefore the workaround described in Note 3 is not applicable here.
6. Fallback to Blt.
7. The wrapper is not strictly necessary here, but works fine.