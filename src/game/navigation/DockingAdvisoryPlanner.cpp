#include "src/game/navigation/DockingAdvisoryPlanner.h"

#include <algorithm>
#include <cmath>
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
        !std::isfinite(r.gateSpacingMeters) || r.gateSpacingMeters <= 0)
    { out.failure = "invalid dock advisory input"; return out; }
    const auto outward = glm::normalize(r.outward);
    const auto stop = r.entranceMeters + outward * r.standoffMeters;
    const auto align = stop + outward * std::max(700.0,3*r.standoffMeters);
    world::navigation::GeometricPathRequest search;
    search.startMeters = r.startMeters;
    search.goalMeters = align;
    search.obstacles = r.obstacles;
    search.params.agentRadiusMeters = r.hullRadiusMeters;
    search.params.maxConsideredObstacles = 48;
    const auto geometry = world::navigation::GeometricPathPlanner::plan(search);
    if (!geometry.valid || geometry.pointsMeters.size() < 2)
    { out.failure = geometry.message; return out; }
    const auto clear = [&](const glm::dvec3& a,const glm::dvec3& b)
    { return world::navigation::segmentClearOfNavigationObstacles(
        a,b,r.obstacles,r.hullRadiusMeters); };
    if (!clear(align,stop))
    { out.failure = "dock alignment blocked"; return out; }
    auto vertices = geometry.pointsMeters;
    vertices.push_back(stop);
    std::vector<glm::dvec3> samples {vertices.front()};
    for (std::size_t i=1;i+1<vertices.size();++i)
    {
        const auto a=vertices[i]-vertices[i-1], b=vertices[i+1]-vertices[i];
        const double la=glm::length(a), lb=glm::length(b);
        if (la < 1e-6 || lb < 1e-6) continue;
        const auto u=a/la,v=b/lb;
        if (glm::dot(u,v)>0.9999) continue;
        double reach=std::min({la*0.4,lb*0.4,
            std::max(20.0,r.maxSpeedMps*r.maxSpeedMps/r.lateralMps2)});
        bool rounded=false;
        for (int attempt=0;attempt<12 && reach>=0.25;++attempt,reach*=0.5)
        {
            const auto entry=vertices[i]-reach*u, exit=vertices[i]+reach*v;
            std::vector<glm::dvec3> arc;
            auto previous=samples.back();
            bool safe=true;
            for (int j=0;j<=20;++j)
            {
                const double t=double(j)/20;
                const auto point=(1-t)*(1-t)*entry+2*t*(1-t)*vertices[i]+t*t*exit;
                if (!clear(previous,point)) {safe=false;break;}
                arc.push_back(point);
                previous=point;
            }
            if (safe) {samples.insert(samples.end(),arc.begin(),arc.end());rounded=true;break;}
        }
        if (!rounded) {out.failure="no clearance to round route";return out;}
    }
    samples.push_back(stop);
    std::vector<double> progress(samples.size(),0.0);
    for (std::size_t i=1;i<samples.size();++i)
    {
        if (!clear(samples[i-1],samples[i]))
        {out.failure="rounded route obstructed";return out;}
        progress[i]=progress[i-1]+glm::length(samples[i]-samples[i-1]);
    }
    if (progress.back()<1.0) {out.failure="route too short";return out;}
    std::vector<DockingAdvisoryGate> dense;
    for (double d=0;d<progress.back();d+=std::min(10.0,r.gateSpacingMeters))
    {
        const auto it=std::upper_bound(progress.begin(),progress.end(),d);
        const std::size_t j=std::clamp<std::size_t>(it-progress.begin(),1,samples.size()-1);
        const double t=(d-progress[j-1])/(progress[j]-progress[j-1]);
        dense.push_back({glm::mix(samples[j-1],samples[j],t),
            glm::normalize(samples[j]-samples[j-1]),r.maxSpeedMps});
    }
    dense.push_back({stop,glm::normalize(stop-samples[samples.size()-2]),0.0});
    for (std::size_t i=1;i+1<dense.size();++i)
    {
        const double angle=std::acos(std::clamp(
            glm::dot(dense[i-1].forward,dense[i+1].forward),-1.0,1.0));
        const double span=glm::length(dense[i+1].positionMeters-dense[i-1].positionMeters);
        if (angle>1e-6)
            dense[i].speedMps=std::min(dense[i].speedMps,
                std::sqrt(r.lateralMps2*span/angle));
    }
    for (std::size_t i=dense.size()-1;i>0;--i)
    {
        const double ds=glm::length(dense[i].positionMeters-dense[i-1].positionMeters);
        dense[i-1].speedMps=std::min(dense[i-1].speedMps,
            std::sqrt(dense[i].speedMps*dense[i].speedMps+2*r.brakingMps2*ds));
    }
    out.gates.push_back(dense.front());
    std::size_t previous=0;
    while (previous+1<dense.size())
    {
        std::size_t next=previous+1;
        while(next+1<dense.size() &&
            glm::length(dense[next].positionMeters-dense[previous].positionMeters)
                < r.gateSpacingMeters) ++next;
        while(next>previous+1 && !clear(dense[previous].positionMeters,dense[next].positionMeters)) --next;
        if(!clear(dense[previous].positionMeters,dense[next].positionMeters))
        {out.failure="display gate chord obstructed";out.gates.clear();return out;}
        auto gate=dense[next];
        for(std::size_t j=previous+1;j<=next;++j)
            gate.speedMps=std::min(gate.speedMps,dense[j].speedMps);
        out.gates.push_back(gate);
        previous=next;
    }
    return out;
}
} // namespace game::navigation
