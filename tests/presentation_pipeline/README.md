# Client presentation pipeline regression tests

Run from the repository root:

```bash
bash tests/presentation_pipeline/run_mingw64.sh
```

This suite freezes the accepted Hub Motion Lab presentation behavior without
requiring OpenGL or a live server process. It verifies:

- one shared snapshot bracket/alpha per render frame;
- no newest-snapshot hold under the accepted 50 Hz simulation / 16.67 Hz
  snapshot / ~80 Hz render cadence;
- exact linear remote interpolation on the delayed timeline;
- recovery from the captured long-client-frame/server-debt disagreement;
- fractional local prediction presentation removing the 50 Hz target staircase.

For an end-to-end live capture after networking, clock, prediction, reference
frame or render-loop changes, enable Hub Motion Lab telemetry and run:

```bash
python tests/hub_motion_lab/verify_capture.py hub_motion_lab_presentation.csv
```
