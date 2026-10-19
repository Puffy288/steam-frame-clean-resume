# Steam Frame Clean Resume (No dashboard on resume)

You’re in your relaxing environment of choice. You need to get up and do something, so you press **Power** to put your Steam Frame to sleep (suspend). When you come back and wake it, the first thing you see is the dashboard.

I don’t want to come back to a menu. I want to be right back in the nice chill environment where I left off.

**Steam Frame Clean Resume** takes you straight back to your running environment when you wake, with the dashboard closed. The **Steam button** still brings up the dashboard whenever you want it.

**Theater mode and floating windows are not affected.** If you put the Frame to sleep while playing a game in theater mode, your theater screen and floating windows are still there, ready to go when you resume. It only closes the dashboard. It does not close your game, theater mode or floating windows.

Unofficial installer for keeping the SteamVR dashboard closed on resume. *Created with AI assistance*

## Install

1. On your **Frame**, select [**Download Steam-Frame-Resume-Install.run**](https://github.com/Puffy288/steam-frame-clean-resume/releases/download/v0.2.0/Steam-Frame-Resume-Install.run) and save it in **Downloads**.
2. Open **Konsole**, then copy and paste this whole command:

```bash
bash ~/Downloads/Steam-Frame-Resume-Install.run
```

3. Select **Enter** on the on-screen keyboard. Wait for **Installed**. You can close Konsole afterwards.

Running this same installer again does not create duplicates. No administrator password is needed.

### Never used Desktop or Konsole on Frame?

Start by opening this GitHub page in **Chromium on your Frame**, so you can download the installer and copy the command there.

With the default Frame controls, point and pull the **trigger** to select. Press the **thumbstick inward** for the context menu, then select an item with the trigger. Use the VR window’s **keyboard button** for the on-screen keys.

1. **Open Chromium.** Press the controller’s **Steam button**, then select **+ (Launch Program) → Desktop** in the dashboard dock. Open the desktop’s **application menu at the bottom left**, search for **Chromium** and open it. If you see **Install Chromium**, select it, follow its Install button, then open Chromium.
2. **Open this page and download the installer.** Select Chromium’s address bar, enter **https://github.com/Puffy288/steam-frame-clean-resume** using the on-screen keyboard, and select **Enter**. In this page’s **Install** section, select **Download Steam-Frame-Resume-Install.run** and save it in **Downloads**.
3. **Open Konsole and copy the command.** Open the desktop’s application menu again, search for **Konsole** (with a **K**) and open it. Return to this GitHub page in Chromium. In the **Install** section, hold the trigger and drag across the whole command to select it. Release the trigger, press the thumbstick inward over the selection, then select **Copy**. You can also press the **copy button at the right of the command box** to copy the whole command.
4. **Paste and install.** Select the dark area in Konsole with the trigger, press the thumbstick inward, and select **Paste**. Open the window’s on-screen keyboard and select **Enter**. Wait for **Installed**.

![Konsole open in the Steam Frame dashboard](docs/screenshots/konsole-on-frame.jpg)

The keyboard button is below the window, on the left of its small control bar.

## Confirm it worked

Leave your VR environment or game running. Press **Power** to suspend, then wake normally. You return to your running environment with the dashboard closed. If you were playing in theater mode or using floating windows, they remain in place. Afterwards, press **Steam** and check that the dashboard opens and stays open until you close it.

## What it does

A small background helper waits for SteamOS’s resume event and sends SteamVR’s built-in dashboard-close request **once**. It does not poll or keep the headset awake. It leaves tracking, boundaries, graphics, game launching and Steam settings alone.

**Measured idle RAM:** about **0.3 MB (0.002% of Frame RAM)** for the helper, with no measurable idle CPU use. Linux’s whole-service reading after resume was **4.5 MB (0.028%)**, including about **4 MB of reclaimable file cache**. Measured on SteamOS 0.5.6, 19 October 2026; readings vary.

The installer and helper make **no internet requests or uploads** and need no administrator password or debugging mode. The installer includes the compiled native helper and its readable C source. Python and a compiler are not needed to install or run it. It briefly reads the existing SteamVR log to check for dashboard use, without sending it anywhere.

<details>
<summary>Exactly where it installs</summary>

`~` means your home folder. Everything persistent goes inside it:

- `~/.local/share/frame-resume-dashboard/`: native helper, C source, uninstaller and ownership marker.
- `~/.config/systemd/user/frame-resume-dashboard.service`: startup service.
- `~/.config/systemd/user/default.target.wants/frame-resume-dashboard.service`: startup link.

Setup removes its temporary `/tmp/frame-resume-dashboard.*` folder on exit. The downloaded installer stays in Downloads. Status messages go to the normal user service journal. No existing Steam configuration is edited, so none needs restoring.

</details>

## Uninstall

Copy this into Konsole using the same controller steps:

```bash
bash ~/.local/share/frame-resume-dashboard/uninstall.sh
```

Select **Enter** on the on-screen keyboard. This stops and removes the helper, service and startup link. Normal dashboard-on-resume behaviour returns. The downloaded installer remains as an ordinary, inactive file. Delete it in the file manager if you want.

Tested with an actual suspend/resume cycle and normal Steam-button dashboard access on SteamOS 0.5.6, 19 October 2026. Reinstallation and uninstall were also verified.

Controller walkthrough and screenshot adapted from [SteamVR Home on Steam Frame](https://github.com/Puffy288/steamvr-home-frame/blob/main/START-HERE.md). Navigation checked against [Valve’s Frame guide](https://help.steampowered.com/en/faqs/view/2668-0B6E-79E0-A85B).
