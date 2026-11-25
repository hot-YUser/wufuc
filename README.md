# wufuc
[![Latest Release](https://img.shields.io/github/v/release/hot-YUser/wufuc.svg?style=flat-square)][Latest]
[![All Releases](https://img.shields.io/github/downloads/hot-YUser/wufuc/total.svg?style=flat-square)][Latest]
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=flat-square)](COPYING)

[🇩🇪][de] [🇪🇸][es] [🇫🇷][fr] [🇮🇹][it] [🇭🇺][hu] [🇧🇷][br] [🇷🇺][ru] [🇨🇳][cn] [🇹🇼][tw]

Disables the "Unsupported Hardware" message in Windows Update, and allows you to continue installing updates on Windows 7 and 8.1 systems with Intel Kaby Lake, AMD Ryzen, or other unsupported processors.

> **Note:** This repository is a restored archive and maintained fork of the original `zeffy/wufuc` project (originally created by zeffy), including the original `v1.0.1.201` release, full branch/tag history, wiki translations, and minor bug/SDK build fixes on the `micro` branch.

## Downloads

**[Latest stable build][Latest] - Most people will want this version.**

## Background

The release notes for Windows updates KB4012218 and KB4012219 included the following:

> Enabled detection of processor generation and hardware support when PC tries to scan or download updates through Windows Update.

These updates marked the implementation of a [policy change](https://blogs.windows.com/windowsexperience/2016/01/15/windows-10-embracing-silicon-innovation/) they announced some time ago, where Microsoft stated that they would not be supporting Windows 7 or 8.1 on next-gen Intel, AMD and Qualcomm processors.

This is essentially a big middle finger to anyone who decides to not "upgrade" to Windows 10,
and it is especially unfortunate considering the extended support periods for Windows 7 and 8.1 won't be ending until January 14, 2020 and January 10, 2023 respectively.

Some people with older Intel and AMD processors were also affected! Users reported the following CPUs being blocked from receiving updates:

- Intel Atom Z530
- Intel Atom D525
- Intel Core i5-M 560
- Intel Core i5-4300M
- Intel Core i7-4930K
- Intel Pentium B940
- AMD FX-6300
- AMD FX-8350
- AMD Turion 64 Mobile Technology ML-34

## Features

- Enables Windows Update on PCs with unsupported processors.
- Written in C, the best programming language. :sunglasses:
- Completely free (as in freedom) software.
- Does not modify any system files.
- Byte pattern-based patching, which means it will usually keep working even after new updates come out.
- No dependencies.

## Frequently Asked Questions

See [FAQ.md](FAQ.md).

## How wufuc works

The tl;dr version is basically:

* Inside a system file called `wuaueng.dll`, there are two functions responsible for the CPU check: `IsDeviceServiceable` and `IsCPUSupported`.
* `IsDeviceServiceable` simply calls `IsCPUSupported` once, and then saves the result and re-uses it on subsequent calls.
* wufuc takes advantage of this behavior by patching the saved result so that it is always `TRUE`, or supported.

If you would like more information, you can read the original write-up on discovering the CPU check [here](https://github.com/hot-YUser/wufuc/tree/old-kb4012218-19).

## Building

To build wufuc from source, you need to download and install the following:

1. [Visual Studio](https://visualstudio.microsoft.com/) (2017 or newer, with C++ desktop development workload and Windows SDK).
2. (Optional, for MSI packages) [Advanced Installer](https://www.advancedinstaller.com/).

## Sponsors (Original Project)

### [Advanced Installer](https://www.advancedinstaller.com/)

The original installer packages were created with Advanced Installer using an [open source license](https://www.advancedinstaller.com/free-license.html).

## Special thanks

- **zeffy** for creating the original `wufuc` project and discovering the CPU check mechanism.
- [**wj32**](https://github.com/wj32) for his awesome program [Process Hacker](https://github.com/processhacker2/processhacker), and also for his [phnt headers](https://github.com/processhacker2/processhacker/tree/master/phnt).
- [**mrexodia**](https://github.com/mrexodia) for [x64dbg](https://github.com/x64dbg/x64dbg), its [`patternfind.cpp`](https://github.com/x64dbg/x64dbg/blob/development/src/dbg/patternfind.cpp) algorithm, and its issue template which was adapted for this project.

[Latest]: https://github.com/hot-YUser/wufuc/releases/latest
[de]: https://github.com/hot-YUser/wufuc/wiki/README-(Deutsch)
[es]: https://github.com/hot-YUser/wufuc/wiki/README-(Espa%C3%B1ol)
[fr]: https://github.com/hot-YUser/wufuc/wiki/README-(Fran%C3%A7ais)
[it]: https://github.com/hot-YUser/wufuc/wiki/README-(Italiano)
[hu]: https://github.com/hot-YUser/wufuc/wiki/README-(Magyar)
[br]: https://github.com/hot-YUser/wufuc/wiki/README-(Portugu%C3%AAs-Brasileiro)
[ru]: https://github.com/hot-YUser/wufuc/wiki/README-(%D1%80%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B9)
[cn]: https://github.com/hot-YUser/wufuc/wiki/README-(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)
[tw]: https://github.com/hot-YUser/wufuc/wiki/README-(%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87)
