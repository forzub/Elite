#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-budget}"
FILE="${2:-herd_500.csv}"

case "${MODE}" in
  shell|budget) ;;
  *)
    echo "[ship-lod-csv] unsupported mode: ${MODE}" >&2
    exit 2
    ;;
esac

if [[ ! -s "${FILE}" ]]; then
  echo "[ship-lod-csv] FAIL: missing or empty ${FILE}" >&2
  exit 2
fi

awk -F',' -v mode="${MODE}" '
BEGIN {
    rows = 0
    failed = 0
    max_full = 0
    max_materialized = 0
    max_slot_error = 0
    cheap_seen = 0
}
NR == 1 {
    expected = "time_s,entities,shells,materialized,state_valid,route_only,kinematic,response,full_control,active,prewarm,coarse,other,mean_slot_error_m,max_slot_error_m,mean_speed_mps"
    if ($0 != expected) {
        print "[ship-lod-csv] FAIL: unexpected CSV header" > "/dev/stderr"
        failed = 1
    }
    next
}
{
    rows += 1

    entities = $2 + 0
    shells = $3 + 0
    materialized = $4 + 0
    state_valid = $5 + 0
    route_only = $6 + 0
    kinematic = $7 + 0
    response = $8 + 0
    full_control = $9 + 0

    if (entities != 500) {
        printf("[ship-lod-csv] FAIL row %d: entities=%d expected=500\n", NR, entities) > "/dev/stderr"
        failed = 1
    }

    if (shells + materialized != entities) {
        printf("[ship-lod-csv] FAIL row %d: shells+materialized=%d entities=%d\n", NR, shells + materialized, entities) > "/dev/stderr"
        failed = 1
    }

    if (state_valid != materialized) {
        printf("[ship-lod-csv] FAIL row %d: state_valid=%d materialized=%d\n", NR, state_valid, materialized) > "/dev/stderr"
        failed = 1
    }

    if (route_only + kinematic + response + full_control != entities) {
        printf("[ship-lod-csv] FAIL row %d: motion census=%d entities=%d\n", NR, route_only + kinematic + response + full_control, entities) > "/dev/stderr"
        failed = 1
    }

    low14 = tolower($14)
    low15 = tolower($15)
    low16 = tolower($16)
    if (low14 ~ /nan|inf/ || low15 ~ /nan|inf/ || low16 ~ /nan|inf/) {
        printf("[ship-lod-csv] FAIL row %d: non-finite diagnostics\n", NR) > "/dev/stderr"
        failed = 1
    }

    if (full_control > max_full)
        max_full = full_control
    if (materialized > max_materialized)
        max_materialized = materialized
    if (($15 + 0) > max_slot_error)
        max_slot_error = $15 + 0

    if (route_only + kinematic + response > 0)
        cheap_seen = 1

    if (mode == "shell") {
        if (shells != 500 || materialized != 0 ||
            route_only != 500 || kinematic != 0 ||
            response != 0 || full_control != 0) {
            printf("[ship-lod-csv] FAIL row %d: shell baseline census changed\n", NR) > "/dev/stderr"
            failed = 1
        }
    }
}
END {
    if (rows < 2) {
        printf("[ship-lod-csv] FAIL: only %d diagnostic row(s); keep the runtime alive long enough to sample more than one epoch\n", rows) > "/dev/stderr"
        failed = 1
    }

    if (mode == "budget") {
        if (!cheap_seen) {
            print "[ship-lod-csv] FAIL: budget run never retained a cheap motion tier" > "/dev/stderr"
            failed = 1
        }
        if (max_full >= 500 || max_materialized >= 500) {
            printf("[ship-lod-csv] FAIL: LOD collapsed to all-expensive state max_full=%d max_materialized=%d\n", max_full, max_materialized) > "/dev/stderr"
            failed = 1
        }
    }

    if (failed)
        exit 2

    printf("[ship-lod-csv] PASS mode=%s rows=%d max_full=%d max_materialized=%d max_slot_error_m=%.3f\n",
           mode, rows, max_full, max_materialized, max_slot_error)
}
' "${FILE}"
