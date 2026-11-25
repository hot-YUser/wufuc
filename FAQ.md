# Frequently Asked Questions

## How to deploy wufuc using Group Policy

[There is a tutorial on the Advanced Installer website that explains how to do this](http://www.advancedinstaller.com/user-guide/tutorial-gpo.html).

## How to use unattended feature in the batch setup scripts

`install_wufuc.bat` and `uninstall_wufuc.bat` both support two command line parameters that can be used alone, or combined to change the behavior of the scripts:

- `/NORESTART` - Automatically declines rebooting after the setup finishes.
- `/UNATTENDED` - Skips all prompts for user interaction, and automatically restarts unless `/NORESTART` is also specified.

These must be used from an elevated command line prompt.

## Compatibility with BypassESU, UpdatePack7R2, and WuaCpuFix

- **BypassESU (WU ESU Patcher / `wuaueng3.dll`)**: `wufuc` v1.0.1+ reads `wuauserv`'s `ServiceDll` dynamically on each service start and can coexist with `BypassESU`'s `wuaueng3.dll` (when `wuaueng3.dll` is built from the original `wuaueng.dll`). **Do not** merge `Restore_wuauserv.reg` if you want to keep `BypassESU`'s WU ESU Patcher active.
- **simplix UpdatePack7R2 (`wuaueng2.dll`)**: UpdatePack7R2 already includes its own CPU check patch via `wuaueng2.dll`. Choose either UpdatePack7R2's built-in patch or `wufuc` — do not stack both. (If you also use `BypassESU` together with `UpdatePack7R2`, `BypassESU` automatically uses `wuaueng2.dll` as its base to create `wuaueng3.dll`, so `wufuc` is not needed.)
- **WuaCpuFix (`WuaCpuFix64.dll`)**: Incompatible with `wufuc`. Uninstall `WuaCpuFix` first, or merge `Restore_wuauserv.reg` and restart your PC before using `wufuc`.

## How to restore Windows Update to its default configuration

If you have previously used another third-party solution that replaced `wuauserv`'s `ServiceDll` (such as `UpdatePack7R2` or `WuaCpuFix`) and want to revert back to the default `wuaueng.dll`:

- Remove the other third-party solution yourself, or
- Merge the included `Restore_wuauserv.reg` file with your registry by double-clicking it, and then restart your PC. *(Note: do not use `Restore_wuauserv.reg` if you rely on `BypassESU`'s WU ESU Patcher.)*

## How to manually remove wufuc v0.8.0.143 when it is impossible to uninstall it normally

This applies exclusively to a very buggy version of wufuc that was only available for download for a short period of time, other versions are unaffected.

1. [Boot into Safe Mode with Command Prompt](https://support.microsoft.com/en-us/help/17419/windows-7-advanced-startup-options-safe-mode).
2. In the command prompt type `regedit` and press enter.
3. Navigate to the key `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options`
4. Expand the `Image File Execution Options` tree.
5. Locate the `svchost.exe` sub key, right-click it and press **Delete**.
6. Reboot, and you should be able to log in normally again.
7. Open Add and Remove Programs, locate and run the normal wufuc uninstaller to complete the removal process.
