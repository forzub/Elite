#!/usr/bin/env bash
# Stage acceptance flights (real game, nav-stress:simple): healthy and damaged
# Newtonian / Assisted, narrow fixture, mid-route capability changes.
# Each line of FLIGHTS: TAG MODEL SECONDS [ENV=VALUE ...]. Summary per flight:
# corridor violations, max cross-track, route status / failures, terminal DONE,
# damage / capability-change / hand-back log lines.
# usage: acceptance.sh RUN [TAG...]   (no TAG: all flights)
# output: $ELITE_DIAG_DIR/elite_RUN_acceptance.txt
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
RUN=$1; shift
ONLY=" $* "
NARROW="$(cygpath -w "$NEWTONIAN_TOOLS/scenarios/narrow_blue_volumes.json")"
FLIGHTS=$(cat <<EOF
n1_healthy Newtonian-I 300
nx_healthy Newtonian-X 300
a3_healthy Assisted-I 200
n1_narrow Newtonian-I 330 ELITE_NAVIGATION_VOLUMES_OVERRIDE=$NARROW
nx_narrow Newtonian-X 330 ELITE_NAVIGATION_VOLUMES_OVERRIDE=$NARROW
n1_main50 Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=60_ship_newtonian_i_main_center_0.5
n1_er5_70 Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=60_ship_newtonian_i_drive_center_reactor_0.7
n1_rcs_y60 Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=60_rcs_fore_upper_left_y_0.6
n1_reactors Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=60_reactors-toggle_0;75_reactors-toggle_0
n1_gas60 Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=30_rcs_gas_tank_0.6
n1_combined Newtonian-I 330 ELITE_SHIP_DAMAGE_AT=60_ship_newtonian_i_main_center_0.5;60_rcs_aft_lower_right_x_0.7
nx_3mains Newtonian-X 330 ELITE_SHIP_DAMAGE_AT=60_ship_newtonian_x_main_upper_left_0.95
a3_engine_lost Assisted-I 200 ELITE_SHIP_DAMAGE_AT=60_ship_engine_L_0.95
a3_reactor60 Assisted-I 200 ELITE_SHIP_DAMAGE_AT=60_ship_drive_left_reactor_0.6
a3_asym2 Assisted-I 200 ELITE_SHIP_DAMAGE_AT=60_ship_engine_L_0.95;60_ship_fore_engine_R_0.95
EOF
)
OUT="$ELITE_DIAG_DIR/elite_${RUN}_acceptance.txt"
: > "$OUT"
cd "$ELITE_ROOT"
while read -r TAG MODEL DURATION ENVS; do
  [ -z "$TAG" ] && continue
  [ "$ONLY" != "  " ] && [[ "$ONLY" != *" $TAG "* ]] && continue
  # ENV values use '_' for spaces inside one damage event (T MODULE TARGET).
  ENVLIST=()
  for kv in $ENVS; do
    k=${kv%%=*}; v=${kv#*=}
    [ "$k" = ELITE_SHIP_DAMAGE_AT ] && v=$(echo "$v" | sed -E 's/^([0-9.]+)_/\1 /; s/;([0-9.]+)_/;\1 /g; s/_([0-9.]+)(;|$)/ \1\2/g')
    ENVLIST+=("$k=$v")
  done
  F="${RUN}_${TAG}"
  env ELITE_TRACE_RUNTIME=1 "${ENVLIST[@]}" bash "$NEWTONIAN_TOOLS/fly.sh" "$MODEL" "$DURATION" "$F" > /dev/null 2>&1
  {
    echo "== $TAG ($MODEL, ${DURATION}s) ${ENVLIST[*]}"
    if [ -f "$ELITE_DIAG_DIR/elite_${F}_decision.tsv" ]; then
      python "$NEWTONIAN_TOOLS/decision.py" "$F" 2>&1 | sed -n '1,3p'
      python "$NEWTONIAN_TOOLS/terminal.py" "$F" 2>&1 | grep -E 'DONE at' | head -1
    fi
    grep -ohE 'status=[A-Z_]+|failures=[0-9]+|max_cross_track_m=[0-9.]+' "$ELITE_DIAG_DIR/elite_$F.log" 2>/dev/null | sort | uniq -c | tr '\n' ' '; echo
    # Frames > 80 ms after the first 40 (startup excluded), max route cross-track.
    echo "hitches(after start)=$(grep '^\[HITCH\]' "$ELITE_DIAG_DIR/elite_$F.stderr" 2>/dev/null | awk -F'frame=' '{split($2,a," "); if (a[1]+0>600) n++} END{print n+0}')"       "max_xtrack_m=$(grep -o 'cross_track_m=[-0-9.e]*' "$ELITE_DIAG_DIR/elite_$F.stderr" 2>/dev/null | awk -F= '{v=$2<0?-$2:$2; if(v>m)m=v} END{printf "%.1f", m}')"
    grep -hE '\[ShipDamageAt\]|\[CapabilityChange\]|execution-replan|capability-ready|hand(ed)? ?back|FAILED|failed=' \
      "$ELITE_DIAG_DIR/elite_$F.stderr" "$ELITE_DIAG_DIR/elite_$F.stdout" 2>/dev/null | cut -c1-200 | sort | uniq -c | head -8
  } >> "$OUT"
done <<< "$FLIGHTS"
cat "$OUT"
