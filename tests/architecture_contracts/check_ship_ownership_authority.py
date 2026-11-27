#!/usr/bin/env python3
"""Ship ownership / authority / audience contract (author 2026-11-27).

ShipOwnershipRegistry (GameServer::m_shipOwnership) is the ONLY owner truth: ShipInstanceId ->
ShipOwnerRef::player(PlayerId) / organization. ownerActor (one shared actor of every player) takes part in NO
authority check. The refit right is checked by the TARGET's owner BEFORE any charge; boarding changes control, never
the owner; F7 gets "owned by this player" from the server; every per-session copy path projects the ship rows.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
failures = []


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def function_body(src, signature):
    start = src.find(signature)
    if start < 0:
        return None
    brace = src.find("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[brace:i + 1]
    return None


server = text("src/game/server/GameServer.cpp")
refit = text("src/game/simulation/GameSimulationRefit.cpp")
replication = text("src/game/simulation/GameSimulationReplication.cpp")
services = text("src/game/SpaceStateServices.cpp")
schema = text("src/game/network/WireDataSchema.h")
refit_snapshot = text("src/game/simulation/ShipRefitSnapshot.h")

# 1. no ownerActor authority anywhere it used to decide
for name, src in (("GameSimulationRefit.cpp", refit), ("SpaceStateServices.cpp", services)):
    if "ownerActor" in src:
        failures.append(f"{name} still reads ownerActor (ownership comes from ShipOwnershipRegistry)")
if "sameOwner" in refit:
    failures.append("GameSimulationRefit.cpp: sameOwner (ownerActor comparison) is back")
if re.search(r"ownedFleet\s*=[^\n]*ownerActor", re.sub(r"//[^\n]*", "", replication)):
    failures.append("GameSimulationReplication.cpp: the owned-fleet gate reads ownerActor")
if "ownerActor" in refit_snapshot.split("struct ShipRefitSnapshot", 1)[1].split("};", 1)[0].replace("// ownerActor was", ""):
    failures.append("ShipRefitSnapshot carries an owner id again")
if re.search(r"ShipRefitSnapshot,[^;]*ownerActor", schema):
    failures.append("WireDataSchema: refit ownerActor rides the wire")

# 2. the refit authority runs before the preview and before chargeRefit
dispatch = server[server.find("case ClientShipCommand::RefitPreview:"):]
dispatch = dispatch[:dispatch.find("break;\n                }") + 1]
a, c, p = dispatch.find("refitAuthority("), dispatch.find("chargeRefit("), dispatch.find("requestRefitPreview(")
if a < 0 or c < 0 or p < 0 or not (a < c and a < p):
    failures.append("refit dispatch: refitAuthority must precede requestRefitPreview and chargeRefit")
auth = function_body(server, "std::string GameServer::refitAuthority(")
if not auth or "ownerPlayerOf" not in auth or "ownerActor" in auth:
    failures.append("refitAuthority must decide by ownerPlayerOf (ShipOwnershipRegistry), never ownerActor")

# 3. boarding: ownership checked, never written
transfer = function_body(server, "std::string GameServer::transferPlayerControl(")
if not transfer:
    failures.append("transferPlayerControl not found")
else:
    if "ownerActor !=" in transfer or "ownerActor ==" in transfer:
        failures.append("transferPlayerControl decides by ownerActor")
    if "m_shipOwnership.assign" in transfer or "assignShipOwner" in transfer:
        failures.append("transferPlayerControl writes the owner (boarding changes control only)")
    if "ownerPlayerOf" not in transfer:
        failures.append("transferPlayerControl does not check the owner")

# 4. a purchase registers the buyer as the owner
buy = function_body(server, "std::string GameServer::buyShip(")
if not buy or "assignShipOwner" not in buy:
    failures.append("buyShip does not record the buyer in ShipOwnershipRegistry")
elif "(void)assignShipOwner" in buy:
    failures.append("buyShip drops the result of assignShipOwner (the sale must fail and refund)")
# 4b. the instance is registered where the ship APPEARS (F7 and navigation read the same instance before boarding)
if not buy or "registerShipInstance(" not in buy or buy.find("registerShipInstance(") > buy.find("assignShipOwner"):
    failures.append("buyShip must register the ShipInstanceRegistry record before the owner (not at the first boarding)")
hire = function_body(server, "std::string GameServer::hireJob(")
if not hire or "registerShipInstance(" not in hire:
    failures.append("hireJob does not register the employer ship's instance")

# 5. persistence: the owner rides the ship save and comes back
save = function_body(server, "void GameServer::saveRuntimeShips()")
restore = function_body(server, "void GameServer::restoreRuntimeShipsIfDue()")
if not save or "owner_player" not in save:
    failures.append("saveRuntimeShips does not write owner_player")
if not restore or "owner_player" not in restore or "owner_unresolved" not in restore:
    failures.append("restoreRuntimeShipsIfDue does not restore owner_player / keep unresolved legacy rows")
if restore and ("owner_organization" not in restore or "ShipOwnerRef::organization" not in restore):
    failures.append("restoreRuntimeShipsIfDue does not restore owner_organization (the save writes it)")
if restore and "registerShipInstance(" not in restore:
    failures.append("restoreRuntimeShipsIfDue does not register the restored instance")
if restore and "absentOwner" not in restore:
    failures.append("restore: a known owner player absent from the slots must be kept typed (absentOwnerPlayer)")
if save and "absentOwnerPlayer" not in save:
    failures.append("saveRuntimeShips does not write the absent owner player back")
if save:
    w = save.find("writeFileAtomically(")
    d = save.find("m_shipsDirty = false", w)
    if w < 0 or d < 0:
        failures.append("saveRuntimeShips: the dirty flag must be cleared only AFTER a successful write")

# 5b. owner / pilot audience are independent facts
proj = function_body(server, "void GameServer::projectShipRowsForSession(")
if proj:
    b = proj.find("blankPrivateShipDomains(")
    o = proj.find("if (!owns)")
    if b < 0 or o < 0 or b > o:
        failures.append("projectShipRowsForSession: the pilot domains must be blanked by 'flies' independently of ownership")

# 6. every per-session copy path projects the ship rows
for fn in ("bool GameServer::copySnapshotForSession(", "bool GameServer::copyHydratedSnapshotForSession(",
           "bool GameServer::copySparseSnapshotForSession("):
    body = function_body(server, fn)
    if not body or "projectShipRowsForSession(" not in body:
        failures.append(f"{fn.split('::')[1]} does not apply projectShipRowsForSession")

# 7. durable accounts
runtime = text("src/game/server/ServerRuntime.cpp")
if "saveAccounts()" not in runtime or "accounts.json" not in runtime:
    failures.append("ServerRuntime: accounts are not persisted (the same PlayerId after a restart)")

if failures:
    for f in failures:
        print("FAIL:", f)
    sys.exit(1)
print("ship ownership authority contract OK")
