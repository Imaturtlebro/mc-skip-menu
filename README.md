# mc-skip-menu

Simple hook for **Minecraft Bedrock (Windows UWP)** that skips the main menus: it boots the client and drives its own UI (via UI Automation) to press **Play -> Play Selected World**, landing you straight in your last world.

Inspired by [morluto/rea](https://github.com/morluto/rea) - but this version needs **zero game addresses**, so it survives game updates.

## Build

Fully cloud-built: every push runs [.github/workflows/build.yml](.github/workflows/build.yml) on GitHub Actions (windows-latest + MSVC). Download the `mc-skip-menu` artifact from the Actions tab - or push a tag for an automatic release.

## Use

1. Download the artifact, extract `hook.dll` + `launcher.exe` anywhere (must have write access).
2. Close any open Minecraft instance.
3. Run `launcher.exe` **as Administrator** (required to open the UWP AppContainer process).

It will:
- start Minecraft via `shell:AppsFolder\MICROSOFT.MINECRAFTUWP_8wekyb3d8bbwe!App`
- wait for `Minecraft.Windows.exe`
- inject `hook.dll`
- the hook waits ~12s for the main menu, then clicks **Play** and **Play Selected World**

## Notes

- UI Automation reads button names, so it works across updates without reverse engineering. If a major UI rework renames buttons, edit `src/hook/main.cpp` and push - Actions rebuilds.
- Purely client-side UI driving via the game's own accessibility tree - it does not tamper with game logic.

Based on your Minecraft install at `C:\Users\Dillo\AppData\Local\Packages\MICROSOFT.MINECRAFTUWP_8wekyb3d8bbwe`.
