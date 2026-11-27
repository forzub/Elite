HANTED - CLOSED PREVIEW BUILD
=============================

This is an EXPERIMENTAL preview of a game in development. It is not a release:
expect rough edges, missing features and bugs. Please do not redistribute.
See BUILD_INFO.txt for the exact build.


1. STARTING THE GAME
--------------------
- Unpack the whole folder anywhere (any disk, any path) and keep its contents
  together. Nothing is installed.
- Double-click Hanted.exe.
- Single player runs entirely on your computer (the game starts its own local
  server internally). No internet connection is needed.
- HantedServer.exe is an OPTIONAL dedicated server for network play; you do not
  need it for single player.
- Settings and logs are kept in %LOCALAPPDATA%\HantedPreview\ .
  Delete that folder to reset the preview's settings.
- Language: English by default. ESC menu -> settings, or Ctrl+Alt+F12 in
  flight, switches the language.


2. SYSTEM REQUIREMENTS (preliminary)
------------------------------------
- Windows 10 or 11, 64-bit.
- A discrete GPU with OpenGL 4.3 support and current drivers
  (tested: NVIDIA Quadro RTX 3000). Integrated graphics are untested.
- 16 GB RAM recommended; about 6 GB of free disk space.
- Microsoft Edge WebView2 Runtime (the menus use it). It is part of
  Windows 11 and of current Windows 10; if the main menu stays blank, install
  "Microsoft Edge WebView2 Runtime" from Microsoft.
- Screen: 1920x1080 or 2560x1440 (higher resolutions are not supported yet).
- On laptops make sure the game runs on the discrete GPU
  (Windows Settings -> Display -> Graphics).


3. FLIGHT CONTROLS
------------------
Attitude
  W / S            pitch down / up
  A / D            roll left / right
  Q / E            yaw left / right
Thrust and speed
  HOME             main thrust on
  END              brake
  INSERT           thrust off
  + / -            raise / lower the speed setpoint (also numpad + / -)
  Numpad 8 / 2     translate forward / back
  Numpad 4 / 6     translate left / right
  Numpad 9 / 3     translate up / down
Flight modes and systems
  Ctrl+J           J drive (in-system fast travel) on / off
  Ctrl+K           stabilisation
  Backspace        free look / free rotation
  G                landing gear up / down
  PgDn             request landing on the selected pad
  Ctrl+L           release the docking clamps (after a landing)
  Ctrl+U           refuel (when landed at a pad)
  Ctrl+R           repair (when landed / docked)
  Left Ctrl+Q      eject the cockpit capsule (emergency)
  P                ship details
  ESC              menu (pause, settings, quit)


4. FUNCTION KEYS F1-F12
-----------------------
  F1   cockpit - front view
  F2   cockpit - rear view
  F3   recon drone camera (when the drone is launched)
  F4   bottom camera (landing / cargo)
  F5   civic & government services   (docked / landed)
  F6   shipyard                      (docked / landed)
  F7   repair & refit                (docked / landed)
  F8   trade                         (docked / landed)
  F9   galaxy map
  F10  star system map
  F11  details map (the area around a body or station)
  F12  local / hub map (the city, its pads and docks)
Pressing the same key again or F1 returns to the cockpit.


5. TARGETS, MAPS AND THE AUTOPILOT
----------------------------------
- Open a map (F9-F12). Click an object to select it; the panel shows its data.
- Right-click an object for its menu: SHOW ON HUD marks it as a target in the
  cockpit; ROUTE plans a flight to it.
- In route planning, START engages the autopilot along the planned route.
  The cockpit shows "AUTOPILOT -> <target>" while it flies.
- Any manual flight input takes the ship back from the autopilot.
- Landing at a city pad: select the pad on the local map (F12), fly near
  it, lower the gear (G) and request landing (PgDn). The pad's dispatcher
  answers in the cockpit and its landing service brings the ship down.


6. KNOWN LIMITATIONS
--------------------
- Preview quality: physics, navigation, economy and the world simulation are
  the current development versions and are still being tuned.
- A few rare messages or names may still appear in Russian.
- Trading needs a station with an economy (e.g. Vysokiy near Earth); some
  hubs, like the Ring, have none yet.
- The ship's drawn attitude can wobble slightly in fast turns (known issue,
  being worked on).
- The world is not saved between sessions in this preview.
- Network play (HantedServer.exe / joining another computer) is not part of
  this preview's tested scenario.
- 4K and ultra-wide screens are not supported yet; cockpit text may overlap
  at some resolutions.


7. IF SOMETHING GOES WRONG - DIAGNOSTIC REPORT
---------------------------------------------
Every session writes a log file:
    %LOCALAPPDATA%\HantedPreview\logs\
(paste that path into the Explorer address bar). Please send the newest
client_*.log file (or all files from the time of the problem), together
with a short description of what you did and, if possible, a screenshot.
After a crash the log contains the crash address and module.
