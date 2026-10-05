# HubCot on Windows XP SP2/Vista/7/8/10/11

HubCot is a series USB Mascots (Toro, Hello Kitty) made by Dreams come true CO.,LTD.

It came with drivers and tools for Windows 98, so it definitely doesn't work with Windows 11 due to driver incompatibility.
This project revives the HubCot hardware by sending USB requests directly with generic WinUSB driver.

### Basic Information

**Project Language**: Visual C
**Project Version**: Visual Studio 2010
**Additional SDK Required:** WinDDK 7600.16385.1

# System Requirements

**Operation System:** Windows XP SP2 or above (Supported by WinUSB)

**Platform:** x86 or x64(AMD64)

Supported Devices: Toro (VID\_0540\&PID\_1B59), Hello Kitty (VID\_0D74\&PID\_D001)

# How to Use

1. Download and use Zadig ([https://zadig.akeo.ie/](https://zadig.akeo.ie/)) to install WinUSB driver for HubCot device using configuration file from the Zadig directory
2. Plug the HubCot device to PC AFTER driver installation
3. Extract the download archive and run HubCot.exe according to your system platform
4. When you press a key on the keyboard, the HubCot will also follow you and type
5. To end connection with HubCot, run HubCot.exe again

# Extensibility

HubCotServer.exe itself is a standalone server application that listens to Windows message and interface with the HubCot hardware.

After you run HubCotServer.exe, you will be able to find a window handle with Class Name "HubCotServerMessageWindowClass".

With the window handle, you can send the following messages (wParam is speed in ms, 0 = 1000):

WM\_USER(0x400) + 0x1	Move Right hand

WM\_USER(0x400) + 0x2	Move Left hand

WM\_USER(0x400) + 0x3	Move Both hands

WM\_USER(0x400) + 0x4	Move Both hands 4 times

WM\_USER(0x400) + 0x5	Move Left hand, then Right hand, repeat for 3 times

WM\_USER(0x400) + 0x6	Move Right hand 3 times

WM\_USER(0x400) + 0x8	Move Both hands 3 times

WM\_USER(0x400) + 0xb	Move Right hand, then Left hand

WM\_USER(0x400) + 0xc	Move Right hand, then Left hand, repeat for 3 times

WM\_QUIT(0x12)	End Server Process (Same as running HubCotServer.exe again)

# Acknowledgements

Thanks Ching-Lan HUANG [(https://github.com/digdog/ToroAlerts](https://github.com/digdog/ToroAlerts)) for creating ToroAlerts on Mac platform, which inspires me from referencing the code and bring Windows XP and newer support.

