# Assisted follower centering recovery point — 2026-10-14

This commit is an intentional hard recovery point before changing in-corridor
cross-track centering semantics.

Known-good state at this boundary:

- Assisted safe-course obstacle correction works in live manual verification.
- Assisted safe-course uses ordinary physical controls; FA-on remains authoritative.
- Assisted and Newtonian production planner/autopilot ownership is isolated.
- BLUE mandatory route geometry is authored through the volume centerline.
- Existing follower keeps the authored tangent while inside its tracking corridor.
- Out-of-corridor CourseCaptureGuidance remains the emergency recovery mechanism.

Known issue intentionally NOT fixed in this recovery point:

- A craft can enter route execution with ~50 m cross-track offset and keep that
  offset because in-corridor steering uses the exact tangent until the full
  tracking tolerance is exceeded.

Do not fold the upcoming centering change into this commit.
