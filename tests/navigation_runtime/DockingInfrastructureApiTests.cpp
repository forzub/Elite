#include "src/game/docking/landing/DockLandingControllerApi.h"
#include "src/game/docking/traffic/DockTrafficControllerApi.h"

#include <iostream>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

game::docking::DockFacilityDescriptor mediumFacility()
{
    using namespace game::docking;

    DockFacilityDescriptor facility;
    facility.facilityId = "hub-medium";
    facility.systemId = 1;
    facility.kinematicFrameId = "hub-frame";

    AccessPortalDescriptor portal;
    portal.id = "portal-m";
    portal.sizeClass = DockSizeClass::Medium;
    portal.usableWidthMeters = 60.0;
    portal.usableHeightMeters = 35.0;
    portal.holdPointId = "hold-m";
    portal.conflictResourceIds = {"portal-m-zone"};
    facility.portals.push_back(portal);

    EntryHoldPointDescriptor hold;
    hold.id = "hold-m";
    hold.portalId = "portal-m";
    hold.positionLocalMeters = {500.0, 0.0, 0.0};
    facility.holdPoints.push_back(hold);

    ParkingPadDescriptor pad;
    pad.id = "pad-m-01";
    pad.sizeClass = DockSizeClass::Medium;
    pad.usableWidthMeters = 60.0;
    pad.usableLengthMeters = 90.0;
    pad.verticalClearanceMeters = 40.0;
    pad.maxMassKg = 200000.0;
    pad.conflictResourceIds = {"pad-m-01-zone"};
    facility.pads.push_back(pad);
    return facility;
}

game::docking::DockingEnvelope mediumShip()
{
    game::docking::DockingEnvelope envelope;
    envelope.widthMeters = 36.0;
    envelope.heightMeters = 18.0;
    envelope.lengthMeters = 55.0;
    envelope.sweptWidthMeters = 40.0;
    envelope.sweptHeightMeters = 22.0;
    envelope.landingFootprintWidthMeters = 30.0;
    envelope.landingFootprintLengthMeters = 45.0;
    envelope.landingGearClearanceMeters = 3.0;
    envelope.massKg = 80000.0;
    return envelope;
}

} // namespace

int main()
{
    try
    {
        using namespace game::docking;
        using namespace game::docking::traffic;

        DockTrafficController traffic(mediumFacility());

        DockingTrafficRequest remote;
        remote.shipId = 101;
        remote.shipClass = DockSizeClass::Medium;
        remote.envelope = mediumShip();
        remote.etaToFacilitySeconds = 300.0;
        remote.insideControlledApproachHorizon = false;

        const auto provisional = traffic.admit(remote);
        require(!provisional.committed, "far request reserved a queue slot");
        require(provisional.queueToken == 0, "far request received a queue token");
        require(
            traffic.inquire(remote).queueLength == 0,
            "far request mutated the dispatcher queue"
        );

        auto near = remote;
        near.insideControlledApproachHorizon = true;
        const auto first = traffic.admit(near);
        require(first.committed, "near request was not admitted");
        require(first.queuePosition == 1, "first admitted ship is not queue position 1");

        near.shipId = 102;
        const auto second = traffic.admit(near);
        require(second.committed, "second near request was not admitted");
        require(second.queuePosition == 2, "queue ordering is not stable");

        require(
            DockTrafficController::classPolicyAllows(
                DockSizeClass::Medium,
                DockSizeClass::Large,
                true
            ),
            "emergency larger-class fallback was rejected"
        );
        require(
            !DockTrafficController::classPolicyAllows(
                DockSizeClass::Medium,
                DockSizeClass::Large,
                false
            ),
            "normal allocation incorrectly consumed a larger dock"
        );
        require(
            !DockTrafficController::classPolicyAllows(
                DockSizeClass::Large,
                DockSizeClass::Medium,
                true
            ),
            "emergency policy allowed a smaller dock"
        );

        AccessPortalDescriptor tooSmallPortal;
        tooSmallPortal.sizeClass = DockSizeClass::Small;
        tooSmallPortal.usableWidthMeters = 20.0;
        tooSmallPortal.usableHeightMeters = 10.0;
        ParkingPadDescriptor tooSmallPad;
        tooSmallPad.sizeClass = DockSizeClass::Small;
        tooSmallPad.usableWidthMeters = 25.0;
        tooSmallPad.usableLengthMeters = 35.0;
        require(
            !DockTrafficController::physicalFit(
                mediumShip(),
                tooSmallPortal,
                tooSmallPad
            ),
            "physical incompatibility was overridden"
        );

        game::docking::landing::DockLandingController landing;
        game::docking::landing::LandingHandoff handoff;
        handoff.valid = true;
        handoff.clearanceRevision = 1;
        handoff.facilityId = "hub-medium";
        handoff.padId = "pad-m-01";
        handoff.kinematicFrameId = "hub-frame";
        handoff.approachPositionLocalMeters = {0.0, 10.0, 0.0};
        handoff.padCenterLocalMeters = {0.0, 0.0, 0.0};
        handoff.padNormalLocal = {0.0, 1.0, 0.0};
        handoff.forwardLocal = {1.0, 0.0, 0.0};
        handoff.positionToleranceMeters = 0.5;
        handoff.linearVelocityToleranceMps = 0.2;
        handoff.angularVelocityToleranceRadPerSec = 0.05;

        require(landing.begin(handoff), "valid LandingHandoff was rejected");
        require(
            landing.state() ==
                game::docking::landing::LandingControllerState::AlignOverPad,
            "landing did not enter terminal alignment state"
        );
        require(
            !game::docking::landing::DockLandingController::
                mainEnginePermitted(),
            "landing API permits main-engine authority"
        );

        // ---- lifecycle (author 2026-11-07): purpose, entry-or-hold, fairness ----
        {
            using game::docking::traffic::DockTrafficController;
            using game::docking::traffic::DockTrafficPhase;
            auto facility = mediumFacility();
            facility.pads.front().purposes = {game::docking::DockPurposeCargo};
            game::docking::ParkingPadDescriptor pad2 = facility.pads.front();
            pad2.id = "pad-m-02";
            pad2.purposes = {game::docking::DockPurposePassenger};
            facility.pads.push_back(pad2);
            DockTrafficController lifecycle(facility);
            game::docking::traffic::DockingTrafficRequest r;
            r.shipClass = game::docking::DockSizeClass::Medium;
            r.envelope = mediumShip();
            r.insideControlledApproachHorizon = true;

            r.shipId = 100;
            r.purpose = game::docking::DockPurposePassenger;
            require(lifecycle.entryFreeNow(r), "idle dock does not offer an immediate entry");
            require(lifecycle.requestDocking(r, 0.0, -1.0, true).committed, "direct entry request rejected");
            lifecycle.update(1.0);
            require(lifecycle.status(100).phase == DockTrafficPhase::EntryCleared, "direct entry not cleared on an idle dock");
            require(lifecycle.status(100).padId == "pad-m-02", "purpose ignored: a passenger ship got the cargo place");

            r.shipId = 101;
            r.purpose = game::docking::DockPurposeCargo;
            require(!lifecycle.entryFreeNow(r), "busy entrance offered an immediate entry");
            require(lifecycle.requestDocking(r, 2.0, 0.0, false).committed, "queued request rejected");
            lifecycle.update(3.0);
            require(lifecycle.status(101).phase == DockTrafficPhase::HoldAssigned, "busy dock did not assign a hold");
            require(lifecycle.reportAtHold(101, 10.0), "hold arrival not accepted");
            require(lifecycle.status(101).estimatedWaitSeconds > 0.0, "a waiting ship has no wait estimate");

            // 100 lands; 101 enters; then 100's departure competes with 102 ready at a hold.
            require(lifecycle.reportInsideChamber(100, 20.0) && lifecycle.reportLanded(100, 30.0), "first ship did not land");
            lifecycle.update(31.0);
            require(lifecycle.status(101).phase == DockTrafficPhase::EntryCleared, "hold ship not cleared after the lanes freed");
            require(lifecycle.requestDeparture(100, 32.0), "departure request rejected");
            require(lifecycle.reportInsideChamber(101, 40.0) && lifecycle.reportLanded(101, 50.0), "second ship did not land");
            r.shipId = 102;
            r.purpose.clear();
            require(lifecycle.requestDocking(r, 51.0, 0.0, false).committed, "third request rejected");
            lifecycle.update(52.0);
            lifecycle.reportAtHold(102, 53.0);
            lifecycle.update(54.0);
            // the last clearance was an arrival -> the departure goes first
            require(lifecycle.status(100).phase == DockTrafficPhase::DepartureCleared, "departure starved behind an arrival");
            require(lifecycle.validate().empty(), "dispatcher invariant broken");
        }

        std::cout << "DOCKING INFRASTRUCTURE API TESTS: PASS\n";
        std::cout << " - remote inquiry does not reserve queue/resources\n";
        std::cout << " - committed queue is stable inside the controlled horizon\n";
        std::cout << " - emergency upgrades only to larger physically compatible class\n";
        std::cout << " - landing handoff is separate and forbids main-engine authority\n";
        std::cout << " - place purpose, entry-or-hold for the pilot, arrival/departure alternation\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "DOCKING INFRASTRUCTURE API TESTS: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
