#include "src/game/navigation/DockingAdvisoryPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation
{
DockingAdvisoryPlan DockingAdvisoryPlanner::plan(const DockingAdvisoryRequest& r)
{
    DockingAdvisoryPlan out;
    const auto finite = [](const glm::dvec3& p)
    { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    if (!finite(r.startMeters) || !finite(r.entranceMeters) ||
        !finite(r.outward) || glm::length(r.outward) < 0.9 ||
        !std::isfinite(r.standoffMeters) || r.standoffMeters <= 0 ||
        !std::isfinite(r.hullRadiusMeters) || r.hullRadiusMeters <= 0 ||
        !std::isfinite(r.maxSpeedMps) || r.maxSpeedMps <= 0 ||
        !std::isfinite(r.acceleratingMps2) || r.acceleratingMps2 <= 0 ||
        !std::isfinite(r.brakingMps2) || r.brakingMps2 <= 0 ||
        !std::isfinite(r.lateralMps2) || r.lateralMps2 <= 0 ||
        !std::isfinite(r.gateSpacingMeters) || r.gateSpacingMeters <= 0 ||
        !std::isfinite(r.initialForwardLeadMeters) ||
            r.initialForwardLeadMeters < 0.0 ||
        (r.hasInitialForward &&
            (!finite(r.initialForward) ||
             glm::length(r.initialForward) < 0.9)) ||
        !std::isfinite(r.terminalGateSpacingMeters) ||
            r.terminalGateSpacingMeters <= 0 ||
        !std::isfinite(r.terminalDenseDistanceMeters) ||
            r.terminalDenseDistanceMeters <= 0 ||
        !std::isfinite(r.terminalApproachLengthMeters) ||
            r.terminalApproachLengthMeters < 0 ||
        !std::isfinite(r.terminalTurnSegmentFraction) ||
            r.terminalTurnSegmentFraction <= 0.0 ||
            r.terminalTurnSegmentFraction > 0.90 ||
        !std::isfinite(r.preferredTerminalTurnRadiusMeters) ||
            r.preferredTerminalTurnRadiusMeters < 0.0)
    { out.failure = "invalid dock advisory input"; return out; }

    const auto outward = glm::normalize(r.outward);
    const auto stop = r.entranceMeters + outward * r.standoffMeters;

    const auto clear = [&](const glm::dvec3& a,const glm::dvec3& b)
    { return world::navigation::segmentClearOfNavigationObstacles(
        a,b,r.obstacles,r.hullRadiusMeters); };

    glm::dvec3 routeSearchStart=r.startMeters;
    bool initialForwardLeadActive=false;
    double initialForwardAcceptedLeadMeters=0.0;
    double initialForwardProtectedStraightMeters=0.0;
    if(r.hasInitialForward && r.initialForwardLeadMeters>1.0e-6)
    {
        const glm::dvec3 initialForward=glm::normalize(r.initialForward);
        const double desiredLead=r.initialForwardLeadMeters;
        const double minimumLead=std::min(
            desiredLead,
            std::max(25.0,r.hullRadiusMeters*2.0)
        );

        double acceptedLead=desiredLead;
        if(!clear(
                r.startMeters,
                r.startMeters+initialForward*acceptedLead))
        {
            if(!clear(
                    r.startMeters,
                    r.startMeters+initialForward*minimumLead))
            {
                out.failure="initial forward corridor blocked";
                return out;
            }

            double clearLead=minimumLead;
            double blockedLead=desiredLead;
            for(int i=0;i<24;++i)
            {
                const double probe=0.5*(clearLead+blockedLead);
                if(clear(
                        r.startMeters,
                        r.startMeters+initialForward*probe))
                    clearLead=probe;
                else
                    blockedLead=probe;
            }
            acceptedLead=clearLead;
        }

        routeSearchStart=
            r.startMeters+initialForward*acceptedLead;
        initialForwardLeadActive=true;
        initialForwardAcceptedLeadMeters=acceptedLead;

        // The launch contract needs a visibly straight section through the
        // windshield, but the rest of the lead must remain available for a
        // tangent arc into the geometric route. The previous implementation
        // protected the entire lead and therefore created a hard corner at
        // routeSearchStart.
        initialForwardProtectedStraightMeters=std::min(
            acceptedLead,
            std::max(500.0, acceptedLead*0.50)
        );
    }

    // Only the near-port ingress is semantic hard geometry. The much longer
    // manual-Assisted lead is a preference used to make the terminal turn
    // human-flyable. Treating the whole preferred lead as mandatory made any
    // unrelated obstacle several kilometres in front of the port cancel the
    // route before rerouting even started.
    const double mandatoryApproachLengthMeters=std::max(
        700.0,
        3*r.standoffMeters
    );
    const auto mandatoryAlign =
        stop + outward * mandatoryApproachLengthMeters;
    if (!clear(mandatoryAlign,stop))
    {
        out.failure = "dock mandatory ingress blocked";
        return out;
    }

    const double preferredApproachLengthMeters=std::max(
        mandatoryApproachLengthMeters,
        r.terminalApproachLengthMeters
    );

    double finalApproachLengthMeters=preferredApproachLengthMeters;
    auto align=stop+outward*finalApproachLengthMeters;

    if (!clear(align,stop))
    {
        // Segment-clear is monotonic along one ray: once an obstacle is hit,
        // every longer segment contains the same hit. Find the longest clear
        // prefix instead of rejecting the navigation task. This preserves as
        // much room as physically available for the broad terminal fillet.
        double clearLength=mandatoryApproachLengthMeters;
        double blockedLength=preferredApproachLengthMeters;
        for (int i=0;i<24;++i)
        {
            const double probeLength=
                0.5*(clearLength+blockedLength);
            const auto probeAlign=stop+outward*probeLength;
            if (clear(probeAlign,stop))
                clearLength=probeLength;
            else
                blockedLength=probeLength;
        }

        // Do not place the route endpoint on the exact contact
        // boundary. The visibility graph needs room to connect a support node
        // to the docking-axis join without grazing the same inflated
        // obstacle. Back away into the already-proved clear prefix.
        const double axisRejoinMarginMeters=std::max(
            50.0,
            r.hullRadiusMeters*4.0
        );
        finalApproachLengthMeters=std::max(
            mandatoryApproachLengthMeters,
            clearLength-axisRejoinMarginMeters
        );
        align=stop+outward*finalApproachLengthMeters;
        if(!clear(align,stop))
        {
            finalApproachLengthMeters=mandatoryApproachLengthMeters;
            align=mandatoryAlign;
        }
        out.terminalApproachShortened=true;
    }

    out.terminalApproachLengthMeters=finalApproachLengthMeters;

    world::navigation::GeometricPathRequest search;
    search.startMeters = routeSearchStart;
    search.goalMeters = align;
    search.obstacles = r.obstacles;
    search.params.agentRadiusMeters = r.hullRadiusMeters;
    search.params.maxConsideredObstacles = 48;

    const auto planGeometry = [&](double additionalClearanceMeters,
                                  std::size_t maxConsideredObstacles)
    {
        auto query = search;
        query.params.additionalClearanceMeters =
            std::max(0.0, additionalClearanceMeters);
        query.params.maxConsideredObstacles = maxConsideredObstacles;
        return world::navigation::GeometricPathPlanner::plan(query);
    };

    auto nominalGeometry = planGeometry(0.0, 48);
    // The geometric planner's obstacle cap is a work bound, never proof that
    // the world has no route. Retry against all static obstacles before
    // declaring even the nominal topology unavailable.
    if (!nominalGeometry.valid && r.obstacles.size() > 48)
        nominalGeometry = planGeometry(0.0, 0);

    // A shortened docking-axis join can still be too close to the obstacle
    // that forced the shortening for a discretized visibility graph to reach
    // it robustly. That is not route impossibility. Retreat the join farther
    // toward the port and retry before giving up the navigation task.
    if (!nominalGeometry.valid && out.terminalApproachShortened)
    {
        double axisRetreatMeters=100.0;
        for (int attempt=0; attempt<8 && !nominalGeometry.valid; ++attempt)
        {
            const double candidateLength=std::max(
                mandatoryApproachLengthMeters,
                finalApproachLengthMeters-axisRetreatMeters
            );
            if(candidateLength>=finalApproachLengthMeters-1.0e-6)
                break;

            const auto candidateAlign=stop+outward*candidateLength;
            if(!clear(candidateAlign,stop))
            {
                axisRetreatMeters*=2.0;
                continue;
            }

            search.goalMeters=candidateAlign;
            auto candidateGeometry=planGeometry(0.0,48);
            if(!candidateGeometry.valid && r.obstacles.size()>48)
                candidateGeometry=planGeometry(0.0,0);

            if(candidateGeometry.valid &&
               candidateGeometry.pointsMeters.size()>=2)
            {
                finalApproachLengthMeters=candidateLength;
                align=candidateAlign;
                out.terminalApproachLengthMeters=
                    finalApproachLengthMeters;
                nominalGeometry=std::move(candidateGeometry);
                break;
            }

            if(candidateLength<=mandatoryApproachLengthMeters+1.0e-6)
                break;
            axisRetreatMeters*=2.0;
        }
    }

    if (!nominalGeometry.valid || nominalGeometry.pointsMeters.size() < 2)
    {
        out.failure = nominalGeometry.message.empty()
            ? "no collision-free docking route"
            : nominalGeometry.message;
        return out;
    }

    const auto prependInitialForwardLead =
        [&](std::vector<glm::dvec3> points)
        {
            if(!initialForwardLeadActive)
                return points;

            if(points.empty() ||
               glm::length(points.front()-routeSearchStart)>1.0e-6)
                points.insert(points.begin(),routeSearchStart);
            if(glm::length(points.front()-r.startMeters)>1.0e-6)
                points.insert(points.begin(),r.startMeters);
            return points;
        };

    nominalGeometry.pointsMeters=
        prependInitialForwardLead(
            std::move(nominalGeometry.pointsMeters)
        );

    struct RoundedCandidate
    {
        bool valid = false;
        bool initialTurnPresent = false;
        double initialTurnRadiusMeters = 0.0;
        bool terminalTurnPresent = false;
        double terminalTurnRadiusMeters = 0.0;
        double terminalArcRotationDegrees = 0.0;
        double lengthMeters = 0.0;
        std::string failure;
        std::vector<glm::dvec3> samples;
    };

    const auto roundGeometry =
        [&](const std::vector<glm::dvec3>& geometryPoints,
            double requiredTerminalRadiusMeters,
            const glm::dvec3& finalPoint)
    {
        RoundedCandidate candidate;
        if (geometryPoints.size() < 2)
        {
            candidate.failure = "geometric route has too few points";
            return candidate;
        }

        auto vertices = geometryPoints;
        if (vertices.empty() ||
            glm::length(vertices.back() - finalPoint) > 1.0e-6)
        {
            vertices.push_back(finalPoint);
        }
        candidate.samples = {vertices.front()};

        if (!r.roundTurns)
        {
            candidate.samples = vertices;
            candidate.lengthMeters = 0.0;
            for (std::size_t i = 1; i < candidate.samples.size(); ++i)
            {
                if (!clear(
                        candidate.samples[i - 1],
                        candidate.samples[i]))
                {
                    candidate.failure =
                        "piecewise-straight docking route obstructed";
                    candidate.samples.clear();
                    return candidate;
                }

                candidate.lengthMeters += glm::length(
                    candidate.samples[i] - candidate.samples[i - 1]
                );
            }
            candidate.valid =
                candidate.samples.size() >= 2 &&
                candidate.lengthMeters >= 1.0;
            if (!candidate.valid)
                candidate.failure = "route too short";
            return candidate;
        }

        for (std::size_t i=1;i+1<vertices.size();++i)
        {
            const auto a=vertices[i]-vertices[i-1];
            const auto b=vertices[i+1]-vertices[i];
            const double la=glm::length(a), lb=glm::length(b);
            if (la < 1e-6 || lb < 1e-6)
                continue;

            const auto u=a/la,v=b/lb;
            const double cosTurn=std::clamp(glm::dot(u,v),-1.0,1.0);
            if (cosTurn>0.9999)
                continue;

            const double turnAngle=std::acos(cosTurn);
            const double tangentScale=std::tan(turnAngle*0.5);
            const auto turnNormalRaw=glm::cross(u,v);
            const double turnNormalLength=glm::length(turnNormalRaw);
            if (!std::isfinite(tangentScale) || tangentScale<=1.0e-9 ||
                turnNormalLength<=1.0e-9)
            {
                candidate.failure="degenerate docking turn";
                return candidate;
            }

            const bool terminalTurn=(i+1==vertices.size()-1);
            const double authoredCruiseSpeed =
                0.8 * r.maxSpeedMps;
            const double desiredRadius=std::max({
                20.0,
                authoredCruiseSpeed*authoredCruiseSpeed/r.lateralMps2,
                terminalTurn ? r.preferredTerminalTurnRadiusMeters : 0.0
            });
            // Below this radius the visible bend becomes a low-speed hairpin
            // that an Assisted pilot cannot comfortably follow. Re-route or
            // report no flyable route instead of silently tightening it.
            const double ordinaryCruiseSpeed =
                0.8 * r.maxSpeedMps;
            const double minimumTransitRadius =
                0.50 * ordinaryCruiseSpeed * ordinaryCruiseSpeed /
                r.lateralMps2;
            const double segmentFraction=
                terminalTurn
                    ? r.terminalTurnSegmentFraction
                    : 0.40;
            double tangentDistance=std::min({
                la*segmentFraction,
                lb*segmentFraction,
                desiredRadius*tangentScale
            });

            if(initialForwardLeadActive && i==1)
            {
                const double maximumLaunchCut=std::max(
                    0.0,
                    initialForwardAcceptedLeadMeters-
                        initialForwardProtectedStraightMeters
                );
                tangentDistance=std::min(
                    tangentDistance,
                    maximumLaunchCut
                );

                // If obstacle shortening left no room for a tangent arc, keep
                // the proved straight lead and expose the sharp topology
                // honestly. Normal 500 m launch leads retain hundreds of
                // metres for a smooth first turn.
                if(tangentDistance<0.25)
                {
                    candidate.samples.push_back(vertices[i]);
                    continue;
                }
            }

            if(terminalTurn &&
               requiredTerminalRadiusMeters>0.0 &&
               tangentDistance/tangentScale+1.0e-6 <
                   requiredTerminalRadiusMeters)
            {
                candidate.failure="preferred terminal turn radius unavailable on candidate";
                return candidate;
            }

            bool rounded=false;
            for (int attempt=0;
                 attempt<12 && tangentDistance>=0.25;
                 ++attempt,tangentDistance*=0.5)
            {
                const double radius=tangentDistance/tangentScale;
                if (!terminalTurn && radius + 1.0e-6 <
                    minimumTransitRadius)
                    break;
                if(terminalTurn &&
                   requiredTerminalRadiusMeters>0.0 &&
                   radius+1.0e-6<requiredTerminalRadiusMeters)
                    break;

                const auto entry=vertices[i]-tangentDistance*u;
                const auto exit=vertices[i]+tangentDistance*v;
                const auto turnNormal=turnNormalRaw/turnNormalLength;
                const auto inwardNormal=glm::normalize(
                    glm::cross(turnNormal,u)
                );
                const auto center=entry+radius*inwardNormal;
                const auto startRadial=entry-center;

                const double arcLength=radius*turnAngle;
                const int arcSegments=std::clamp(
                    static_cast<int>(std::ceil(arcLength/10.0)),
                    4,
                    512
                );

                std::vector<glm::dvec3> arc;
                arc.reserve(static_cast<std::size_t>(arcSegments)+1);
                auto previous=candidate.samples.back();
                bool safe=true;
                for (int j=0;j<=arcSegments;++j)
                {
                    const double t=double(j)/double(arcSegments);
                    const double phi=turnAngle*t;
                    const double cp=std::cos(phi);
                    const double sp=std::sin(phi);
                    const auto radial=
                        startRadial*cp+
                        glm::cross(turnNormal,startRadial)*sp+
                        turnNormal*glm::dot(turnNormal,startRadial)*(1.0-cp);
                    const auto point=center+radial;
                    if (!clear(previous,point)) {safe=false;break;}
                    arc.push_back(point);
                    previous=point;
                }
                if (safe && !arc.empty() &&
                    glm::length(arc.back()-exit)<=1.0e-5)
                {
                    candidate.samples.insert(
                        candidate.samples.end(),arc.begin(),arc.end());
                    if(initialForwardLeadActive && i==1)
                    {
                        candidate.initialTurnPresent=true;
                        candidate.initialTurnRadiusMeters=radius;
                    }
                    if (terminalTurn)
                    {
                        candidate.terminalTurnPresent=true;
                        candidate.terminalTurnRadiusMeters=radius;
                    }
                    rounded=true;
                    break;
                }
            }

            if (!rounded)
            {
                candidate.failure =
                    terminalTurn
                        ? "no clearance for terminal circular route fillet"
                        : "no clearance for circular route fillet";
                return candidate;
            }
        }

        candidate.samples.push_back(stop);
        for (std::size_t i=1;i<candidate.samples.size();++i)
        {
            if (!clear(candidate.samples[i-1],candidate.samples[i]))
            {
                candidate.failure="rounded route obstructed";
                candidate.samples.clear();
                return candidate;
            }
            candidate.lengthMeters +=
                glm::length(candidate.samples[i]-candidate.samples[i-1]);
        }

        if (candidate.lengthMeters<1.0)
        {
            candidate.failure="route too short";
            candidate.samples.clear();
            return candidate;
        }

        candidate.valid=true;
        return candidate;
    };

    const double preferredTerminalRadius =
        r.preferredTerminalTurnRadiusMeters;

    const auto estimatedTraversalSeconds =
        [&](const RoundedCandidate& candidate)
        {
            if (!candidate.valid || candidate.samples.size() < 2)
                return std::numeric_limits<double>::infinity();

            const std::size_t count = candidate.samples.size();
            const double cruiseSpeed = std::max(0.5, 0.8 * r.maxSpeedMps);
            std::vector<double> speeds(count, cruiseSpeed);
            speeds.front() = 0.0;
            speeds.back() = 0.0;

            for (std::size_t i = 1; i + 1 < count; ++i)
            {
                const glm::dvec3 a =
                    candidate.samples[i] - candidate.samples[i - 1];
                const glm::dvec3 b =
                    candidate.samples[i + 1] - candidate.samples[i];
                const double la = glm::length(a);
                const double lb = glm::length(b);
                const double across = glm::length(
                    candidate.samples[i + 1] -
                    candidate.samples[i - 1]
                );
                const double denom = la * lb * across;
                if (denom <= 1.0e-9)
                    continue;

                const double curvature =
                    2.0 * glm::length(glm::cross(a, b)) / denom;
                if (curvature > 1.0e-9)
                {
                    speeds[i] = std::min(
                        speeds[i],
                        std::sqrt(r.lateralMps2 / curvature)
                    );
                }
            }

            for (std::size_t i = count - 1; i > 0; --i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                speeds[i - 1] = std::min(
                    speeds[i - 1],
                    std::sqrt(
                        speeds[i] * speeds[i] +
                        2.0 * r.brakingMps2 * ds
                    )
                );
            }

            for (std::size_t i = 1; i < count; ++i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                speeds[i] = std::min(
                    speeds[i],
                    std::sqrt(
                        speeds[i - 1] * speeds[i - 1] +
                        2.0 * r.acceleratingMps2 * ds
                    )
                );
            }

            double seconds = 0.0;
            for (std::size_t i = 1; i < count; ++i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                const double speedSum = speeds[i - 1] + speeds[i];
                if (speedSum <= 1.0e-6)
                    return std::numeric_limits<double>::infinity();
                seconds += 2.0 * ds / speedSum;
            }
            return seconds;
        };

    // Phase 1: routes without a preferred terminal radius keep the ordinary
    // rounded topology. A preferred docking radius uses a different contract:
    // 'align' is the START OF THE FINAL STRAIGHT, not the virtual sharp corner
    // consumed by a fillet.
    RoundedCandidate selected;
    double selectedTraversalSeconds =
        std::numeric_limits<double>::infinity();
    bool detourUsed=false;
    bool radiusRelaxed=false;

    if (preferredTerminalRadius<=0.0)
    {
        selected=roundGeometry(nominalGeometry.pointsMeters,0.0,stop);
        selectedTraversalSeconds=estimatedTraversalSeconds(selected);
    }

    const auto considerPreferredCandidate =
        [&](RoundedCandidate candidate, bool candidateIsDetour)
        {
            if (!candidate.valid)
                return;

            const double candidateSeconds =
                estimatedTraversalSeconds(candidate);
            const bool better =
                !selected.valid ||
                candidateSeconds + 1.0e-6 <
                    selectedTraversalSeconds ||
                (std::abs(
                     candidateSeconds -
                     selectedTraversalSeconds
                 ) <= 1.0e-6 &&
                 candidate.lengthMeters + 1.0e-6 <
                     selected.lengthMeters);

            if (!better)
                return;

            selected = std::move(candidate);
            selectedTraversalSeconds = candidateSeconds;
            detourUsed = candidateIsDetour;
        };

    // Phase 2: author the terminal circular primitive first, then route TO it.
    //
    // HARD CONTRACT:
    //   entry -> exact circular arc(R) -> align -> stop
    //
    // The generic corner-rounding code is NOT allowed to alter R. It only
    // smooths the route that arrives at the straight tangent BEFORE entry.
    // This keeps the requested docking arc a commanded primitive instead of a
    // suggestion that another planner layer may squeeze afterwards.
    world::navigation::GeometricPathResult wideGeometry;
    if (preferredTerminalRadius>0.0)
    {
        const glm::dvec3 finalDirection =
            glm::normalize(stop-align);
        const glm::dvec3 basisSeed =
            std::abs(finalDirection.y)<0.90
                ? glm::dvec3(0.0,1.0,0.0)
                : glm::dvec3(1.0,0.0,0.0);
        const glm::dvec3 lateralA =
            glm::normalize(glm::cross(finalDirection,basisSeed));
        const glm::dvec3 lateralB =
            glm::normalize(glm::cross(finalDirection,lateralA));

        const double authoredCruiseSpeed =
            0.8 * r.maxSpeedMps;
        const double transitComfortRadius=std::max(
            20.0,
            0.50 * authoredCruiseSpeed * authoredCruiseSpeed /
                r.lateralMps2
        );
        const double terminalPrimitiveRadius=std::max({
            20.0,
            authoredCruiseSpeed*authoredCruiseSpeed/r.lateralMps2,
            preferredTerminalRadius
        });

        out.terminalTurnRequestedRadiusMeters =
            terminalPrimitiveRadius;

        // This straight exists only to let the preceding topology turn settle
        // onto the exact terminal-arc tangent. It does NOT size the terminal
        // arc itself.
        const double preArcStraightMeters =
            std::max(1000.0, 2.0 * transitComfortRadius);

        constexpr int terminalIngressSamples=36;
        constexpr double twoPi=
            6.283185307179586476925286766559;
        constexpr double halfPi=
            1.5707963267948966192313216916398;

        out.terminalArcCandidatesTested =
            terminalIngressSamples;

        for(int sample=0;
            sample<terminalIngressSamples;
            ++sample)
        {
            const double angle=
                twoPi*double(sample)/double(terminalIngressSamples);
            const glm::dvec3 incoming=
                lateralA*std::cos(angle)+
                lateralB*std::sin(angle);

            // Exact quarter-circle ending at align:
            //   center = align - incoming*R
            //   entry  = center - finalDirection*R
            //
            // Tangent(entry)=incoming, tangent(align)=finalDirection.
            const glm::dvec3 center =
                align - incoming * terminalPrimitiveRadius;
            const glm::dvec3 entry =
                center - finalDirection * terminalPrimitiveRadius;
            const glm::dvec3 preEntry =
                entry - incoming * preArcStraightMeters;

            auto ingressSearch=search;
            ingressSearch.goalMeters=preEntry;
            ingressSearch.params.maxConsideredObstacles=0;
            const auto ingressGeometry=
                world::navigation::GeometricPathPlanner::plan(ingressSearch);
            if(!ingressGeometry.valid ||
               ingressGeometry.pointsMeters.size()<2)
            {
                ++out.terminalArcRouteRejected;
                out.terminalArcLastRejection =
                    "route-to-pre-entry:" +
                    (ingressGeometry.message.empty()
                        ? std::string("invalid")
                        : ingressGeometry.message);
                continue;
            }
            ++out.terminalArcRouteable;

            auto points=ingressGeometry.pointsMeters;
            points=prependInitialForwardLead(std::move(points));

            auto transitCandidate =
                roundGeometry(points,0.0,entry);
            if(!transitCandidate.valid)
            {
                ++out.terminalArcTransitRejected;
                out.terminalArcLastRejection =
                    "transit-to-entry:" +
                    transitCandidate.failure;
                continue;
            }
            ++out.terminalArcTransitReady;

            RoundedCandidate candidate =
                std::move(transitCandidate);
            candidate.terminalTurnPresent=true;
            candidate.terminalTurnRadiusMeters=
                terminalPrimitiveRadius;
            candidate.terminalArcRotationDegrees =
                angle * 180.0 /
                3.1415926535897932384626433832795;

            const glm::dvec3 turnNormal =
                glm::normalize(glm::cross(incoming,finalDirection));
            const glm::dvec3 startRadial =
                entry-center;
            const double arcLength =
                terminalPrimitiveRadius * halfPi;
            const int arcSegments=std::clamp(
                static_cast<int>(std::ceil(arcLength/10.0)),
                12,
                1024
            );

            bool arcClear=true;
            glm::dvec3 previous=entry;
            for(int j=1;j<=arcSegments;++j)
            {
                const double t=
                    double(j)/double(arcSegments);
                const double phi=halfPi*t;
                const double cp=std::cos(phi);
                const double sp=std::sin(phi);
                const glm::dvec3 radial=
                    startRadial*cp+
                    glm::cross(turnNormal,startRadial)*sp+
                    turnNormal*
                        glm::dot(turnNormal,startRadial)*
                        (1.0-cp);
                glm::dvec3 point=center+radial;
                if(j==arcSegments)
                    point=align;

                if(!clear(previous,point))
                {
                    arcClear=false;
                    break;
                }

                candidate.samples.push_back(point);
                previous=point;
            }

            if(!arcClear || !clear(align,stop))
            {
                ++out.terminalArcCollisionRejected;
                out.terminalArcLastRejection =
                    !arcClear
                        ? "terminal-arc-obstructed"
                        : "final-straight-obstructed";
                continue;
            }

            if(glm::length(candidate.samples.back()-stop)>1.0e-6)
                candidate.samples.push_back(stop);

            candidate.lengthMeters=0.0;
            bool wholeRouteClear=true;
            for(std::size_t i=1;i<candidate.samples.size();++i)
            {
                if(!clear(candidate.samples[i-1],candidate.samples[i]))
                {
                    wholeRouteClear=false;
                    break;
                }
                candidate.lengthMeters +=
                    glm::length(
                        candidate.samples[i]-
                        candidate.samples[i-1]
                    );
            }

            if(!wholeRouteClear)
            {
                ++out.terminalArcCollisionRejected;
                out.terminalArcLastRejection =
                    "combined-route-obstructed";
                continue;
            }

            candidate.valid=true;
            candidate.failure.clear();
            ++out.terminalArcAcceptedCandidates;
            considerPreferredCandidate(
                std::move(candidate),
                true
            );
        }

        // Keep a conventional route only as the explicit radius-relaxation
        // fallback after ALL rotated exact-R candidates have failed.
        wideGeometry=planGeometry(0.0,0);
        if(wideGeometry.valid &&
           wideGeometry.pointsMeters.size()>=2)
        {
            wideGeometry.pointsMeters=
                prependInitialForwardLead(
                    std::move(wideGeometry.pointsMeters)
                );
        }
    }

    // Phase 3: only after reroute has failed may the terminal arc tighten.
    // Start from the same preferred/dynamic radius and halve only as clearance
    // forces it, so the accepted fallback remains the widest locally feasible
    // circular arc rather than cancelling the manual navigation task.
    if (!selected.valid)
    {
        auto relaxed = roundGeometry(nominalGeometry.pointsMeters,0.0,stop);
        if (!relaxed.valid &&
            wideGeometry.valid &&
            wideGeometry.pointsMeters.size()>=2)
        {
            relaxed=roundGeometry(wideGeometry.pointsMeters,0.0,stop);
            if (relaxed.valid)
                detourUsed=true;
        }

        if (!relaxed.valid)
        {
            out.failure = "no collision-free docking route after reroute/tighten fallback";
            return out;
        }

        selected=std::move(relaxed);
        radiusRelaxed=
            selected.terminalTurnPresent &&
            preferredTerminalRadius>0.0 &&
            selected.terminalTurnRadiusMeters+1.0e-6<
                preferredTerminalRadius;
    }

    auto& samples=selected.samples;
    out.terminalDetourUsed=detourUsed;
    out.terminalTurnRadiusRelaxed=radiusRelaxed;
    out.terminalTurnRadiusMeters=selected.terminalTurnRadiusMeters;
    out.terminalArcRotationDegrees=
        selected.terminalArcRotationDegrees;
    out.initialTurnPresent=selected.initialTurnPresent;
    out.initialTurnRadiusMeters=selected.initialTurnRadiusMeters;

    std::vector<double> progress(samples.size(),0.0);
    for (std::size_t i=1;i<samples.size();++i)
        progress[i]=progress[i-1]+glm::length(samples[i]-samples[i-1]);

    std::vector<DockingAdvisoryGate> dense;
    for (double d=0;d<progress.back();d+=std::min(10.0,r.gateSpacingMeters))
    {
        const auto it=std::upper_bound(progress.begin(),progress.end(),d);
        const std::size_t j=std::clamp<std::size_t>(
            it-progress.begin(),1,samples.size()-1);
        const double t=(d-progress[j-1])/(progress[j]-progress[j-1]);
        dense.push_back({
            glm::mix(samples[j-1],samples[j],t),
            glm::normalize(samples[j]-samples[j-1]),
            r.maxSpeedMps * 0.8
        });
    }
    dense.push_back({
        stop,
        glm::normalize(stop-samples[samples.size()-2]),
        0.0
    });

    for (std::size_t i=1;i+1<dense.size();++i)
    {
        const double angle=std::acos(std::clamp(
            glm::dot(dense[i-1].forward,dense[i+1].forward),-1.0,1.0));
        const double span=glm::length(
            dense[i+1].positionMeters-dense[i-1].positionMeters);
        if (angle>1e-6)
            dense[i].speedMps=std::min(
                dense[i].speedMps,
                std::sqrt(r.lateralMps2*span/angle)
            );
    }

    for (std::size_t i=dense.size()-1;i>0;--i)
    {
        const double ds=glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters);
        dense[i-1].speedMps=std::min(
            dense[i-1].speedMps,
            std::sqrt(
                dense[i].speedMps*dense[i].speedMps+
                2*r.brakingMps2*ds
            )
        );
    }

    // The preparation phase starts this route from a stopped ship. Forward
    // limits keep the visible speed recommendation consistent with actual
    // acceleration, while the previous pass reserves braking for every arc.
    dense.front().speedMps = 0.0;
    for (std::size_t i=1;i<dense.size();++i)
    {
        const double ds=glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters);
        dense[i].speedMps=std::min(
            dense[i].speedMps,
            std::sqrt(dense[i-1].speedMps*dense[i-1].speedMps+
                      2*r.acceleratingMps2*ds)
        );
    }

    // Preserve the exact accepted dense route for Automatic execution. The
    // sparse public gates below are only presentation samples of this same
    // product and must never be reinterpreted into a different flight path.
    out.executionGates = dense;

    std::vector<double> denseProgress(dense.size(),0.0);
    for(std::size_t i=1;i<dense.size();++i)
        denseProgress[i]=denseProgress[i-1]+glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters
        );

    out.gates.push_back(dense.front());
    std::size_t previous=0;
    while (previous+1<dense.size())
    {
        std::size_t next=previous+1;

        // USER-CONTRACT: published docking frames use the authored
        // cadence only. Normal guidance is 500 m and terminal guidance is
        // 250 m for the current docking request. Do not inject a special
        // fractional transition frame merely to land exactly on the dense
        // boundary. Start terminal cadence up to one normal interval early so
        // the final dense region is still fully covered.
        const double remainingFromPrevious=
            denseProgress.back()-denseProgress[previous];
        const double terminalSpacing=std::min(
            r.gateSpacingMeters,
            r.terminalGateSpacingMeters
        );
        const double terminalActivationRemaining=
            r.terminalDenseDistanceMeters+r.gateSpacingMeters;

        const double spacingMeters=
            remainingFromPrevious<=terminalActivationRemaining
                ? terminalSpacing
                : r.gateSpacingMeters;

        while(next+1<dense.size())
        {
            const std::size_t candidate=next+1;
            const double routeDistance=
                denseProgress[candidate]-denseProgress[previous];
            if(routeDistance>spacingMeters+1.0e-6)
                break;
            next=candidate;
        }

        while(next>previous+1 &&
              !clear(dense[previous].positionMeters,dense[next].positionMeters))
            --next;
        if(!clear(dense[previous].positionMeters,dense[next].positionMeters))
        {
            out.failure="display gate chord obstructed";
            out.gates.clear();
            return out;
        }

        auto gate=dense[next];
        for(std::size_t j=previous+1;j<=next;++j)
            gate.speedMps=std::min(gate.speedMps,dense[j].speedMps);
        out.gates.push_back(gate);
        previous=next;
    }

    return out;
}
} // namespace game::navigation
