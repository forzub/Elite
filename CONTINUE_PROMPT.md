# CONTINUE PROMPT — verify the Hub-local manual docking route

Latest user instruction: no patch-file delivery; publish code directly to
GitHub and supply `git pull`/check/build/run commands. Read-only remote check
found `origin/main=84d59f4d`. Direct `git push origin main` was rejected by
automatic approval review because the user had not explicitly authorized the
exact public-main publication. Do not bypass via a different Git/GitHub
mechanism and do not claim the fixes are remotely available. Seek express
authorization to publish local commits to public `forzub/Elite` `origin/main`;
then verify remote HEAD before providing download commands. No new runtime
docking result has been observed after corrected code installation.


The Windows target supplied decisive deployment evidence: `HEAD=84d59f4d`
with `origin/main` equal, `build/EliteGame.exe` lacks the corrected
`[DockAdvisory] axis request=` literal, and the complete log search has only
the old bare failure. This is an unpatched runtime, not evidence of a failure
of the Hub-local fix. Install the complete mailbox patch based on `84d59f4d`
via `git am` in `D:\\__elite\\work`, rebuild the canonical game, and verify
the binary marker before flight testing. Preserve the user's untracked trace
JSON/TXT files. Remote publication remains blocked by automatic approval
review; do not bypass it without explicit authorization.


Latest target excerpt: route briefly appears, then bare `request=1
failed=dock moved off approach axis`; no numeric `[DockAdvisory] axis` record
or startup locale line was included. The corrected guard prints axis delta to
the same stderr before the failure. The excerpt may be partial. Before any
more navigation edits, obtain target checkout commit, inspect the built
`build/EliteGame.exe` for the literal `[DockAdvisory] axis request=`, and read
the complete combined `build/test-logs/docking-live.log`. The canonical build
script compiles that executable from its invoking checkout. Do not widen the
2 m guard to conceal unknown deployment identity.


New target blocker: Windows startup aborted on nlohmann JSON numeric-lexer
assertion `endptr == token_buffer.data() + token_buffer.size()`. `main.cpp`
had selected the user's `LC_ALL`; decimal-comma locales make C `strtod` stop
at the JSON dot. The local fix sets only `LC_NUMERIC=C` after `LC_ALL` and
before app/threads, printing `[Startup] LC_NUMERIC=C`. A focused regression
parses fractional/exponent JSON successfully; Windows rerun is pending.
The preceding `session-start-update` duration log is not a crash stack.
Keep the navigation axis test separate until startup is confirmed. Refresh
the local patch; remote public-main publication was rejected by automatic
review and must not be bypassed without explicit authorization.


Latest distribution gate: direct `git push origin main` was rejected by
automatic approval review because it publishes multiple local commits to the
public default branch without explicit approval of that exact action. Do not
attempt indirect publication. The corrected branch is local; remote `main`
still tracks `84d59f4` here. Provide a refreshed `git am` patch for Windows
testing. Explain that `tee` shows a full combined game log in the console and
also writes `build/test-logs/docking-live.log`. Remote publication is pending
explicit user authorization for that action.


Most recent observation: the target again printed only two
`failed=dock moved off approach axis` lines. `origin/main` at `84d59f4`
emits that exact form; the corrected local guard at `81175bb` MUST print
`[DockAdvisory] axis request=... delta_m=...` to the same stderr immediately
before the failure. The supplied excerpt may be filtered or partial. First
verify applied patch/checkout HEAD, rebuilt executable and full merged log.
If the numeric line appears, diagnose that measured local discrepancy; if
absent from an unfiltered run, treat the executable as stale. This follow-up
changes documentation only; the 2 m guard stays intact.


Work in `forzub/Elite` on `main`. First read `AGENTS.md`, the newest dated
sections of `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and the
last dated section of `src/game/navigation/STAGE12_END_TO_END.md`. Update all
four state/stage files and this prompt after each material result.

Latest live observation: SHOW ROUTE briefly appeared, then requests 1 and 2
failed `dock moved off approach axis`. Visibility proves takeover, physical
settling, authoritative planning and publication were reached; the supplied
fragment does not prove server-confirmed Human hand-back. The split world
prediction of terminal gate and dock was corrected in earlier local commits.

Current correction: every planning decision uses one authoritative snapshot
universe epoch, one tactical Hub-local coordinate frame for ship start, port,
obstacles and gates. Runtime samples ship and dock at the same server tick,
compares the unchanged terminal gate with the local semantic port/standoff and
tracks the corridor with the sampled local ship position. Timeline and Hub
identity are checked; the 2 m axis threshold and 500 m gate spacing remain.
The diagnostic cube's authored local translation is zero. Its 2 deg/s spin is
around the entrance normal through the entrance center, so that axis is fixed.

Presentation never changes the local route: the cockpit converts all gates
using the player's render frame/time; the Hub map projects those same local
points directly with its local camera. Manual-mode blinking uses render time.
Planning, frame mismatch, axis, corridor and hand-back logs include tick/time,
Hub identity or measured local error. Native docking and focused architecture
checks pass locally; the full Windows game build and live flight gate are open.
The full runner lacks CMake/Ninja/CTest here; separately running its Python
portion passes 82/91, with nine failures in unrelated areas. Full game syntax
compilation lacks GLAD. Do not report the full architecture gate as passed.

Direct push to public `main` was rejected by automatic approval review. Local
commits are not on origin/main; obtain explicit authorization before retrying
that publication. For target verification use the exported patch locally, then
run `bash tests/architecture_contracts/run_mingw64.sh`, the
`docking_advisory_tests` CMake/CTest target, `bash build_mingw64.sh`, and
`(cd build && ./EliteGame.exe)` from MSYS2 MinGW64. Press SHOW ROUTE while
moving; verify stabilization, a route for >45 s, manual controls after the
server acknowledgement, cancellation on card close and corridor exit. Capture
`phase=stabilizing -> phase=planning -> phase=handoff_wait ->
[DockPrep] published ... human_restored=1 -> phase=manual human_control=1`.
If cancelled, capture the complete `[DockAdvisory] axis` or `left` numeric line.
Automatic DOCKING remains disabled; native tests are not flight acceptance.
