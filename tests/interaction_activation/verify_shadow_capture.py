#!/usr/bin/env python3

import csv
import sys
from collections import Counter, defaultdict
from pathlib import Path

REQUIRED = {
    "server_time_s",
    "entity_id",
    "role",
    "current_mode",
    "desired_mode",
    "reason",
    "requested_mode",
    "planned_mode",
    "plan_transition",
    "transition_serial",
    "last_transition",
    "last_transition_time_s",
    "claim_kind",
    "claim_source_id",
    "broadphase_candidates",
    "broadphase_comparable",
    "broadphase_fallback",
    "broadphase_query_radius_m",
    "broadphase_visited_cells",
    "broadphase_subject_residual_speed_mps",
    "broadphase_max_anchor_residual_speed_mps",
    "npc_ai_eligible",
    "npc_ai_lab",
    "npc_ai_lab_phase",
    "npc_ai_interval_s",
    "npc_ai_time_since_think_s",
    "npc_ai_think_count",
    "npc_ai_skipped_frames",
    "npc_ai_last_think_time_s",
    "anchor_id",
    "anchor_kind",
    "current_center_distance_m",
    "current_surface_distance_m",
    "time_to_closest_s",
    "closest_center_distance_m",
    "closest_surface_distance_m",
    "interaction_envelope_m",
    "within_envelope",
    "enters_horizon",
}

RANK = {"Coarse": 0, "Prewarm": 1, "Active": 2}
TRANSITIONS = {
    "none",
    "promote-prewarm",
    "promote-active",
    "demote-prewarm",
    "demote-coarse",
}


def fail(message: str) -> None:
    print(f"[FAIL] {message}")
    raise SystemExit(1)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_shadow_capture.py simulation_activation_shadow.csv")
        return 2

    path = Path(sys.argv[1])
    if not path.exists():
        fail(f"capture not found: {path}")

    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        missing = REQUIRED.difference(reader.fieldnames or [])
        if missing:
            fail(f"missing columns: {', '.join(sorted(missing))}")
        rows = list(reader)

    if not rows:
        fail("capture contains no rows")

    reasons = Counter()
    desired = Counter()
    requested = Counter()
    planned = Counter()
    transitions = Counter()
    claims = Counter()
    by_entity = defaultdict(Counter)
    previous_planned = {}
    previous_transition_serial = {}
    previous_ai_think_count = {}
    previous_ai_skipped_count = {}
    player_rows = 0
    non_player_rows = 0
    fallback_rows = 0
    total_candidates = 0
    total_comparable = 0
    total_query_radius = 0.0
    total_visited_cells = 0
    total_subject_residual_speed = 0.0
    total_anchor_residual_speed = 0.0
    eligible_ai_rows = 0
    coarse_ai_rows = 0
    coarse_ai_skipped_observed = False
    lab_rows = 0
    lab_modes = Counter()
    lab_phases = Counter()
    lab_intervals = set()

    for index, row in enumerate(rows, start=2):
        entity_id = row["entity_id"]
        current = row["current_mode"]
        wanted = row["desired_mode"]
        request = row["requested_mode"]
        plan = row["planned_mode"]
        transition = row["plan_transition"]
        last_transition = row["last_transition"]
        claim_kind = row["claim_kind"]
        reason = row["reason"]
        within = row["within_envelope"] == "1"
        enters = row["enters_horizon"] == "1"
        server_time = float(row["server_time_s"])
        transition_serial = int(row["transition_serial"])
        last_transition_time = float(row["last_transition_time_s"])
        candidates = int(row["broadphase_candidates"])
        comparable = int(row["broadphase_comparable"])
        fallback = row["broadphase_fallback"] == "1"
        query_radius = float(row["broadphase_query_radius_m"])
        visited_cells = int(row["broadphase_visited_cells"])
        subject_residual_speed = float(
            row["broadphase_subject_residual_speed_mps"]
        )
        anchor_residual_speed = float(
            row["broadphase_max_anchor_residual_speed_mps"]
        )
        ai_eligible = row["npc_ai_eligible"] == "1"
        ai_lab = row["npc_ai_lab"] == "1"
        ai_lab_phase = row["npc_ai_lab_phase"]
        ai_interval = float(row["npc_ai_interval_s"])
        ai_time_since_think = float(row["npc_ai_time_since_think_s"])
        ai_think_count = int(row["npc_ai_think_count"])
        ai_skipped_count = int(row["npc_ai_skipped_frames"])
        ai_last_think_time = float(row["npc_ai_last_think_time_s"])

        # Stage 3E gates only NPC AI think cadence. Full dynamic physics and
        # the rest of the materialized runtime are still Active.
        if current != "Active":
            fail(
                f"row {index}: Stage 3D current_mode must remain Active, got {current}"
            )

        if wanted not in RANK or request not in RANK or plan not in RANK:
            fail(f"row {index}: unexpected activation mode")

        if ai_time_since_think < -1e-9 or ai_last_think_time < -1e-9:
            fail(f"row {index}: negative NPC AI cadence telemetry")
        if ai_last_think_time > server_time + 1e-6:
            fail(f"row {index}: NPC AI last think is in the future")

        if ai_lab:
            lab_rows += 1
            lab_modes[plan] += 1
            lab_phases[ai_lab_phase] += 1
            lab_intervals.add(round(ai_interval, 6))

            if not ai_eligible:
                fail(f"row {index}: activation cadence lab actor is not AI eligible")
            if ai_lab_phase not in {
                "coarse",
                "prewarm-claim",
                "active-claim",
                "release",
            }:
                fail(f"row {index}: unknown activation cadence lab phase")
            if ai_lab_phase == "prewarm-claim":
                if claim_kind != "scripted-critical" or request == "Coarse":
                    fail(f"row {index}: prewarm lab claim did not raise demand")
            if ai_lab_phase == "active-claim":
                if claim_kind != "scripted-critical" or request != "Active" or plan != "Active":
                    fail(f"row {index}: active lab claim did not promote immediately")
            if ai_lab_phase in {"coarse", "release"} and claim_kind == "scripted-critical":
                fail(f"row {index}: released lab phase still has scripted claim")

        if reason == "player-pinned":
            if ai_eligible or any((ai_think_count, ai_skipped_count)):
                fail(f"row {index}: player must not consume NPC AI cadence")
        elif not ai_eligible:
            # Hub Motion Lab actors deliberately bypass production NPC AI so
            # their controlled trajectories remain deterministic probes.
            if any((ai_think_count, ai_skipped_count)):
                fail(f"row {index}: ineligible NPC consumed AI cadence")
        else:
            eligible_ai_rows += 1
            expected_interval = {"Active": 0.0, "Prewarm": 0.1, "Coarse": 1.0}[plan]
            if abs(ai_interval - expected_interval) > 1e-6:
                fail(
                    f"row {index}: NPC AI interval {ai_interval} does not match "
                    f"planned mode {plan}"
                )

            previous_thinks = previous_ai_think_count.get(entity_id)
            previous_skips = previous_ai_skipped_count.get(entity_id)
            if previous_thinks is not None and ai_think_count < previous_thinks:
                fail(f"row {index}: NPC AI think count moved backwards")
            if previous_skips is not None and ai_skipped_count < previous_skips:
                fail(f"row {index}: NPC AI skipped count moved backwards")
            previous_ai_think_count[entity_id] = ai_think_count
            previous_ai_skipped_count[entity_id] = ai_skipped_count

            if plan == "Coarse":
                coarse_ai_rows += 1
                if ai_skipped_count > 0:
                    coarse_ai_skipped_observed = True

        if transition not in TRANSITIONS or last_transition not in TRANSITIONS:
            fail(f"row {index}: unexpected transition value")

        if comparable > candidates:
            fail(f"row {index}: comparable anchors exceed broadphase candidates")

        if query_radius < 0.0 or subject_residual_speed < 0.0 or anchor_residual_speed < 0.0:
            fail(f"row {index}: broadphase diagnostics contain negative values")
        if fallback and visited_cells != 0:
            fail(f"row {index}: fallback query must report zero visited cells")

        if RANK[request] < RANK[wanted]:
            fail(f"row {index}: gameplay claim path lowered physical demand")

        if claim_kind == "none" and request != wanted:
            fail(f"row {index}: request differs from physical demand without a claim")

        if claim_kind != "none" and row["claim_source_id"] == "0":
            fail(f"row {index}: live claim has no source id")

        if reason == "player-pinned":
            player_rows += 1
            if wanted != "Active" or request != "Active" or plan != "Active":
                fail(f"row {index}: player-pinned row is not fully Active")
        else:
            non_player_rows += 1
            fallback_rows += int(fallback)
            total_candidates += candidates
            total_comparable += comparable
            total_query_radius += query_radius
            total_visited_cells += visited_cells
            total_subject_residual_speed += subject_residual_speed
            total_anchor_residual_speed += anchor_residual_speed

        if reason == "current-interaction":
            if wanted != "Active" or not within:
                fail(f"row {index}: current interaction contract mismatch")
            if comparable == 0:
                fail(f"row {index}: interaction has no comparable broadphase anchor")

        if reason == "predicted-interaction":
            if wanted != "Prewarm" or within or not enters:
                fail(f"row {index}: predicted interaction contract mismatch")
            if comparable == 0:
                fail(f"row {index}: predicted interaction has no broadphase anchor")

        if reason in {"no-interaction", "no-comparable-anchors"}:
            if wanted != "Coarse":
                fail(f"row {index}: non-interaction row must request Coarse physically")

        # Promotions must be immediate. Demotions are the only operations that
        # hysteresis may delay.
        if request == "Active" and plan != "Active":
            fail(f"row {index}: Active request was not promoted immediately")
        if request == "Prewarm" and plan == "Coarse":
            fail(f"row {index}: Prewarm request remained Coarse")

        if transition == "promote-active" and plan != "Active":
            fail(f"row {index}: promote-active transition mismatch")
        if transition in {"promote-prewarm", "demote-prewarm"} and plan != "Prewarm":
            fail(f"row {index}: prewarm transition mismatch")
        if transition == "demote-coarse" and plan != "Coarse":
            fail(f"row {index}: demote-coarse transition mismatch")

        previous_serial = previous_transition_serial.get(entity_id)
        if previous_serial is not None and transition_serial < previous_serial:
            fail(f"row {index}: transition serial moved backwards")
        if transition_serial > 0:
            if last_transition == "none":
                fail(f"row {index}: durable transition serial has no last transition")
            if last_transition_time > server_time + 1e-6:
                fail(f"row {index}: last transition is in the future")
        previous_transition_serial[entity_id] = transition_serial

        # Active must never collapse directly to Coarse in the sampled capture.
        previous = previous_planned.get(entity_id)
        if previous == "Active" and plan == "Coarse":
            fail(f"row {index}: direct Active -> Coarse demotion bypassed Prewarm")
        previous_planned[entity_id] = plan

        reasons[reason] += 1
        desired[wanted] += 1
        requested[request] += 1
        planned[plan] += 1
        transitions[transition] += 1
        claims[claim_kind] += 1
        by_entity[entity_id][plan] += 1

    if player_rows == 0:
        fail("capture contains no player-pinned rows")

    # A broad-phase that falls back for most real NPC samples is correct but
    # operationally useless. Keep a deliberately loose runtime gate.
    fallback_ratio = fallback_rows / non_player_rows if non_player_rows else 0.0
    if fallback_ratio > 0.25:
        fail(f"broadphase fallback ratio too high: {fallback_ratio:.3%}")

    if eligible_ai_rows > 0:
        if coarse_ai_rows == 0:
            fail("eligible NPC capture never exercised Coarse AI cadence")
        if not coarse_ai_skipped_observed:
            fail("Coarse NPC AI cadence never skipped a fixed simulation frame")

    if lab_rows == 0:
        fail("capture contains no activation cadence lab actor")
    for mode in ("Coarse", "Prewarm", "Active"):
        if lab_modes[mode] == 0:
            fail(f"activation cadence lab never exercised {mode} planned mode")
    for phase in ("coarse", "prewarm-claim", "active-claim", "release"):
        if lab_phases[phase] == 0:
            fail(f"activation cadence lab never exercised phase {phase}")
    for interval in (0.0, 0.1, 1.0):
        if interval not in lab_intervals:
            fail(f"activation cadence lab never observed AI interval {interval}")

    print(f"rows={len(rows)} entities={len(by_entity)}")
    print(f"npc_ai_eligible_rows={eligible_ai_rows}")
    print(
        "npc_ai_lab="
        + ", ".join(f"{mode}:{count}" for mode, count in sorted(lab_modes.items()))
        + " phases="
        + ", ".join(f"{phase}:{count}" for phase, count in sorted(lab_phases.items()))
    )
    print("physical=" + ", ".join(f"{k}:{v}" for k, v in sorted(desired.items())))
    print("requested=" + ", ".join(f"{k}:{v}" for k, v in sorted(requested.items())))
    print("planned=" + ", ".join(f"{k}:{v}" for k, v in sorted(planned.items())))
    print("transitions=" + ", ".join(f"{k}:{v}" for k, v in sorted(transitions.items())))
    print("claims=" + ", ".join(f"{k}:{v}" for k, v in sorted(claims.items())))
    print("reasons=" + ", ".join(f"{k}:{v}" for k, v in sorted(reasons.items())))
    if non_player_rows:
        print(
            "broadphase="
            f"avg_candidates:{total_candidates / non_player_rows:.2f}, "
            f"avg_comparable:{total_comparable / non_player_rows:.2f}, "
            f"avg_radius_m:{total_query_radius / non_player_rows:.1f}, "
            f"avg_cells:{total_visited_cells / non_player_rows:.1f}, "
            f"avg_subject_residual_mps:{total_subject_residual_speed / non_player_rows:.2f}, "
            f"max_anchor_residual_mps:{total_anchor_residual_speed / non_player_rows:.2f}, "
            f"fallback:{fallback_ratio:.3%}"
        )

    for entity_id in sorted(by_entity, key=lambda value: int(value)):
        states = ", ".join(
            f"{mode}:{count}" for mode, count in sorted(by_entity[entity_id].items())
        )
        print(f"entity {entity_id}: {states}")

    print("[PASS] Stage 3E.1 activation broadphase/planner/live AI cadence capture contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
