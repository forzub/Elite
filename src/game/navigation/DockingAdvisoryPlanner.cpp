#include "src/game/navigation/DockingAdvisoryPlanner.h"

#include <algorithm>
#include <cmath>
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
        !std::isfinite(r.brakingMps2) || r.brakingMps2 <= 0 ||
        !std::isfinite(r.lateralMps2) || r.lateralMps2 <= 0 ||
        !std::isfinite(r.gateSpacingMeters) || r.gateSpacingMeters <= 0 ||
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
    search.startMeters = r.startMeters;
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

    struct RoundedCandidate
    {
        bool valid = false;
        bool terminalTurnPresent = false;
        double terminalTurnRadiusMeters = 0.0;
        double lengthMeters = 0.0;
        std::string failure;
        std::vector<glm::dvec3> samples;
    };

    const auto roundGeometry =
        [&](const std::vector<glm::dvec3>& geometryPoints,
            double requiredTerminalRadiusMeters)
    {
        RoundedCandidate candidate;
        if (geometryPoints.size() < 2)
        {
            candidate.failure = "geometric route has too few points";
            return candidate;
        }

        auto vertices = geometryPoints;
        vertices.push_back(stop);
        candidate.samples = {vertices.front()};

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
            const double desiredRadius=std::max({
                20.0,
                r.maxSpeedMps*r.maxSpeedMps/r.lateralMps2,
                terminalTurn ? r.preferredTerminalTurnRadiusMeters : 0.0
            });
            const double segmentFraction=
                terminalTurn
                    ? r.terminalTurnSegmentFraction
                    : 0.40;
            double tangentDistance=std::min({
                la*segmentFraction,
                lb*segmentFraction,
                desiredRadius*tangentScale
            });

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

    // Phase 1: preserve the human-flyable radius on the nominal route.
    RoundedCandidate selected =
        roundGeometry(
            nominalGeometry.pointsMeters,
            preferredTerminalRadius
        );
    bool detourUsed=false;
    bool radiusRelaxed=false;

    // Phase 2: if the preferred arc collides or cannot be fitted, change the
    // route before changing the radius. First vary the direction from which
    // the ship reaches the docking-axis alignment point. This is the important
    // topological fallback: GeometricPathPlanner may route from the current
    // ship position to any of these pre-alignment points around the entire
    // station, while the final semantic docking axis remains unchanged.
    world::navigation::GeometricPathResult wideGeometry;
    if (!selected.valid && preferredTerminalRadius>0.0)
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

        const double terminalLeadLength=std::max(
            finalApproachLengthMeters,
            preferredTerminalRadius/
                r.terminalTurnSegmentFraction*1.10
        );
        constexpr int terminalIngressSamples=12;
        constexpr double twoPi=
            6.283185307179586476925286766559;

        for(int sample=0;
            sample<terminalIngressSamples && !selected.valid;
            ++sample)
        {
            const double angle=
                twoPi*double(sample)/double(terminalIngressSamples);
            const glm::dvec3 incoming=
                lateralA*std::cos(angle)+
                lateralB*std::sin(angle);
            const glm::dvec3 preAlign=
                align-incoming*terminalLeadLength;

            if(!clear(preAlign,align))
                continue;

            auto ingressSearch=search;
            ingressSearch.goalMeters=preAlign;
            // A route-existence fallback must not turn the normal 48-obstacle
            // work cap into a false proof of impossibility.
            ingressSearch.params.maxConsideredObstacles=0;
            const auto ingressGeometry=
                world::navigation::GeometricPathPlanner::plan(ingressSearch);
            if(!ingressGeometry.valid ||
               ingressGeometry.pointsMeters.size()<2)
                continue;

            auto points=ingressGeometry.pointsMeters;
            if(glm::length(points.back()-align)>1.0e-6)
                points.push_back(align);

            auto detour=roundGeometry(
                points,
                preferredTerminalRadius
            );
            if(detour.valid)
            {
                selected=std::move(detour);
                detourUsed=true;
            }
        }

        // If changing terminal ingress direction still cannot keep the broad
        // arc, widen obstacle clearance on the original topology. This can
        // force visibility/A* support nodes onto the far side of a large
        // station/structure. Try half radius first so a nearby semantic
        // alignment point is not unnecessarily swallowed by inflated geometry.
        for (int reroutePass=0;
             reroutePass<2 && !selected.valid;
             ++reroutePass)
        {
            const double clearanceScale =
                reroutePass==0 ? 0.5 : 1.0;
            auto rerouted = planGeometry(
                preferredTerminalRadius*clearanceScale,
                0
            );
            if (!rerouted.valid || rerouted.pointsMeters.size()<2)
                continue;

            wideGeometry=rerouted;
            auto wide = roundGeometry(
                rerouted.pointsMeters,
                preferredTerminalRadius
            );
            if (wide.valid)
            {
                selected=std::move(wide);
                detourUsed=true;
            }
        }
    }

    // Phase 3: only after reroute has failed may the terminal arc tighten.
    // Start from the same preferred/dynamic radius and halve only as clearance
    // forces it, so the accepted fallback remains the widest locally feasible
    // circular arc rather than cancelling the manual navigation task.
    if (!selected.valid)
    {
        auto relaxed = roundGeometry(nominalGeometry.pointsMeters,0.0);
        if (!relaxed.valid &&
            wideGeometry.valid &&
            wideGeometry.pointsMeters.size()>=2)
        {
            relaxed=roundGeometry(wideGeometry.pointsMeters,0.0);
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
            r.maxSpeedMps
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

        // Anchor the cadence transition explicitly. A sparse 500 m step may
        // never jump across the terminal-density boundary.
        const double remainingFromPrevious=
            denseProgress.back()-denseProgress[previous];
        const double terminalSpacing=std::min(
            r.gateSpacingMeters,
            r.terminalGateSpacingMeters
        );
        const double terminalActivationRemaining=
            r.terminalDenseDistanceMeters+terminalSpacing;

        double spacingMeters=r.gateSpacingMeters;
        if(remainingFromPrevious<=terminalActivationRemaining)
        {
            spacingMeters=terminalSpacing;
        }
        else
        {
            const double distanceToActivation=
                remainingFromPrevious-terminalActivationRemaining;
            if(distanceToActivation<r.gateSpacingMeters)
                spacingMeters=distanceToActivation;
        }

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
