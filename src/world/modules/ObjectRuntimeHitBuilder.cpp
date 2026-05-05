#include "ObjectRuntimeHitBuilder.h"

#include <unordered_set>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <map>
#include <cmath>
#include <limits>
#include <queue>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/norm.hpp>


#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/debug/DebugSettings.h"


using namespace game::damage;
using namespace game::ship::geometry;

namespace world::modules
{

struct EdgeStrip
{
    std::string panelId;
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat3 orientation;
};

struct EdgeStripPair
{
    std::string panelId;
    int edgeIndex = -1;

    glm::vec3 aWorld {0.0f};
    glm::vec3 bWorld {0.0f};
    glm::vec3 axisWorld {1.0f, 0.0f, 0.0f};
    float length = 0.0f;

    EdgeStrip plus;
    EdgeStrip minus;
};

struct RuntimeVolumeBuildResult
{
    bool valid = false;

    glm::vec3 center {0.0f};
    glm::vec3 halfSize {0.5f};
    glm::mat3 orientation {1.0f};

    glm::vec3 aabbMin {0.0f};
    glm::vec3 aabbMax {0.0f};

    bool isObb = false;
};

struct LocalSupportProxy
{
    std::string linkId;
    std::string supportModuleId;

    glm::vec3 localCenter {0.0f};
    glm::vec3 halfSize {0.05f, 0.05f, 0.05f};
    glm::mat3 localOrientation {1.0f};

    float maxHealth = 100.0f;
    float impulseTolerance = 250.0f;

    float score = 0.0f;
};


static glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback = glm::vec3(1.0f, 0.0f, 0.0f))
{
    float l2 = glm::length2(v);
    if (l2 <= 1e-10f)
        return fallback;

    return v / std::sqrt(l2);
}


struct BoundaryEdgeLocal
{
    glm::vec3 aLocal {0.0f};
    glm::vec3 bLocal {0.0f};
    glm::vec3 dirLocal {1.0f, 0.0f, 0.0f};
    glm::vec3 faceNormalLocal {0.0f, 0.0f, 1.0f};
    float length = 0.0f;
};

struct BoundaryEdgeWorld
{
    glm::vec3 aWorld {0.0f};
    glm::vec3 bWorld {0.0f};
    glm::vec3 dirWorld {1.0f, 0.0f, 0.0f};
    glm::vec3 faceNormalWorld {0.0f, 0.0f, 1.0f};
    float length = 0.0f;
};

struct PartRef
{
    const AssemblyModule* module = nullptr;
    const AssemblyMeshPart* part = nullptr;

    glm::mat4 partLocalModel {1.0f};
    glm::mat4 invPartLocalModel {1.0f};
};


struct TempVolume
{
    std::string ownerModuleId;
    glm::vec3 center {0.0f};
    glm::vec3 halfSize {0.05f, 0.05f, 0.05f};
    glm::mat3 orientation {1.0f};
};


struct CachedHitTemplate
{
    std::vector<HitVolume> moduleVolumes;
    std::vector<StructuralLinkState> structuralLinks;
};

static std::unordered_map<int, CachedHitTemplate> s_hitTemplateCache;

static int makeHitTemplateCacheKey(ObjectType typeId)
{
    return static_cast<int>(typeId);
}




static void appendObbCorners(
    const glm::vec3& center,
    const glm::vec3& halfSize,
    const glm::mat3& orientation,
    std::vector<glm::vec3>& out
)
{
    glm::vec3 ax = orientation[0] * halfSize.x;
    glm::vec3 ay = orientation[1] * halfSize.y;
    glm::vec3 az = orientation[2] * halfSize.z;

    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                out.push_back(center + float(sx) * ax + float(sy) * ay + float(sz) * az);
}

static bool obbIntersect(
    const glm::vec3& cA,
    const glm::vec3& eA,
    const glm::mat3& A,
    const glm::vec3& cB,
    const glm::vec3& eB,
    const glm::mat3& B
)
{
    const float EPS = 1e-5f;

    glm::vec3 Au[3] = {
        safeNormalize(A[0], glm::vec3(1,0,0)),
        safeNormalize(A[1], glm::vec3(0,1,0)),
        safeNormalize(A[2], glm::vec3(0,0,1))
    };

    glm::vec3 Bu[3] = {
        safeNormalize(B[0], glm::vec3(1,0,0)),
        safeNormalize(B[1], glm::vec3(0,1,0)),
        safeNormalize(B[2], glm::vec3(0,0,1))
    };

    float R[3][3];
    float AbsR[3][3];

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            R[i][j] = glm::dot(Au[i], Bu[j]);
            AbsR[i][j] = std::fabs(R[i][j]) + EPS;
        }
    }

    glm::vec3 tWorld = cB - cA;
    glm::vec3 t(
        glm::dot(tWorld, Au[0]),
        glm::dot(tWorld, Au[1]),
        glm::dot(tWorld, Au[2])
    );

    // A axes
    for (int i = 0; i < 3; ++i)
    {
        float ra = eA[i];
        float rb = eB[0] * AbsR[i][0] + eB[1] * AbsR[i][1] + eB[2] * AbsR[i][2];
        if (std::fabs(t[i]) > ra + rb)
            return false;
    }

    // B axes
    for (int j = 0; j < 3; ++j)
    {
        float ra = eA[0] * AbsR[0][j] + eA[1] * AbsR[1][j] + eA[2] * AbsR[2][j];
        float rb = eB[j];
        float tj = std::fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);
        if (tj > ra + rb)
            return false;
    }

    // cross axes Ai x Bj
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            float ra =
                eA[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
                eA[(i + 2) % 3] * AbsR[(i + 1) % 3][j];

            float rb =
                eB[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
                eB[(j + 2) % 3] * AbsR[i][(j + 1) % 3];

            float tij = std::fabs(
                t[(i + 2) % 3] * R[(i + 1) % 3][j] -
                t[(i + 1) % 3] * R[(i + 2) % 3][j]
            );

            if (tij > ra + rb)
                return false;
        }
    }

    return true;
}

static void computeClusterObb(
    const std::vector<TempVolume>& tempVolumes,
    const std::vector<int>& cluster,
    glm::vec3& outCenter,
    glm::vec3& outHalfSize,
    glm::mat3& outOrientation
)
{
    outOrientation = tempVolumes[cluster.front()].orientation;
    outOrientation[0] = safeNormalize(outOrientation[0], glm::vec3(1,0,0));
    outOrientation[1] = safeNormalize(outOrientation[1], glm::vec3(0,1,0));
    outOrientation[2] = safeNormalize(outOrientation[2], glm::vec3(0,0,1));

    std::vector<glm::vec3> corners;
    for (int idx : cluster)
    {
        appendObbCorners(
            tempVolumes[idx].center,
            tempVolumes[idx].halfSize,
            tempVolumes[idx].orientation,
            corners
        );
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    glm::vec3 origin(0.0f);

    for (const auto& p : corners)
    {
        float x = glm::dot(p, outOrientation[0]);
        float y = glm::dot(p, outOrientation[1]);
        float z = glm::dot(p, outOrientation[2]);

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        maxZ = std::max(maxZ, z);
    }

    outHalfSize = glm::vec3(
        (maxX - minX) * 0.5f,
        (maxY - minY) * 0.5f,
        (maxZ - minZ) * 0.5f
    );

    glm::vec3 localCenter(
        (maxX + minX) * 0.5f,
        (maxY + minY) * 0.5f,
        (maxZ + minZ) * 0.5f
    );

    outCenter =
        outOrientation[0] * localCenter.x +
        outOrientation[1] * localCenter.y +
        outOrientation[2] * localCenter.z;
}

static std::string makeSortedSeamId(
    const std::string& a,
    const std::string& b,
    int clusterIndex
)
{
    if (a < b)
        return "panel_seam::" + a + "::" + b + "::" + std::to_string(clusterIndex);

    return "panel_seam::" + b + "::" + a + "::" + std::to_string(clusterIndex);
}




static bool stripsIntersect(const EdgeStrip& a, const EdgeStrip& b)
{
    glm::vec3 axisA = safeNormalize(a.orientation[0], glm::vec3(1,0,0));
    glm::vec3 axisB = safeNormalize(b.orientation[0], glm::vec3(1,0,0));

    const float axisDot = std::fabs(glm::dot(axisA, axisB));

    // Шов — это почти параллельные колбаски.
    // Перекрёстные диагонали нам не нужны.
    if (axisDot < 0.82f)
        return false;

    if (!obbIntersect(
            a.center, a.halfSize, a.orientation,
            b.center, b.halfSize, b.orientation))
    {
        return false;
    }

    glm::vec3 axis = axisA;
    if (glm::dot(axisA, axisB) < 0.0f)
        axisB = -axisB;

    axis = safeNormalize(axisA + axisB, axisA);

    glm::vec3 anchor = (a.center + b.center) * 0.5f;

    float a0 = glm::dot((a.center - axis * a.halfSize.x) - anchor, axis);
    float a1 = glm::dot((a.center + axis * a.halfSize.x) - anchor, axis);
    float b0 = glm::dot((b.center - axis * b.halfSize.x) - anchor, axis);
    float b1 = glm::dot((b.center + axis * b.halfSize.x) - anchor, axis);

    float minA = std::min(a0, a1);
    float maxA = std::max(a0, a1);
    float minB = std::min(b0, b1);
    float maxB = std::max(b0, b1);

    float overlapStart = std::max(minA, minB);
    float overlapEnd   = std::min(maxA, maxB);

    const float overlap = std::max(0.0f, overlapEnd - overlapStart);

    // Если пересеклись только кончиками — это не нормальный шов.
    if (overlap < 0.07f)
        return false;

    glm::vec3 delta = b.center - a.center;
    glm::vec3 lateral = delta - axis * glm::dot(delta, axis);
    const float lateralDistance = glm::length(lateral);

    const float maxAllowedDistance =
        a.halfSize.y + a.halfSize.z +
        b.halfSize.y + b.halfSize.z +
        0.16f;

    if (lateralDistance > maxAllowedDistance)
        return false;

    return true;
}




static glm::vec3 closestPointOnSegmentSimple(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b
)
{
    glm::vec3 ab = b - a;
    float len2 = glm::dot(ab, ab);

    if (len2 <= 1e-8f)
        return a;

    float t = glm::dot(p - a, ab) / len2;
    t = std::max(0.0f, std::min(1.0f, t));

    return a + ab * t;
}



static bool tryBuildSeamFromEdgePairs(
    const std::string& panelA,
    const std::string& panelB,
    const EdgeStripPair& edgeA,
    const EdgeStripPair& edgeB,
    int seamIndex,
    StructuralLinkState& outSeam
)
{
    static int seamDebugCounter = 0;

    glm::vec3 axisA = safeNormalize(edgeA.axisWorld, glm::vec3(1,0,0));
    glm::vec3 axisB = safeNormalize(edgeB.axisWorld, glm::vec3(1,0,0));

    float rawDot = glm::dot(axisA, axisB);
    float axisDot = std::fabs(rawDot);

    // Настоящий шов должен идти примерно в одном направлении.
    if (axisDot < 0.84f)
        return false;

    if (rawDot < 0.0f)
        axisB = -axisB;

    glm::vec3 seamAxis = safeNormalize(axisA + axisB, axisA);

    glm::vec3 anchor =
        (edgeA.aWorld + edgeA.bWorld + edgeB.aWorld + edgeB.bWorld) * 0.25f;

    float a0 = glm::dot(edgeA.aWorld - anchor, seamAxis);
    float a1 = glm::dot(edgeA.bWorld - anchor, seamAxis);
    float b0 = glm::dot(edgeB.aWorld - anchor, seamAxis);
    float b1 = glm::dot(edgeB.bWorld - anchor, seamAxis);

    float minA = std::min(a0, a1);
    float maxA = std::max(a0, a1);
    float minB = std::min(b0, b1);
    float maxB = std::max(b0, b1);

    float overlapStart = std::max(minA, minB);
    float overlapEnd   = std::min(maxA, maxB);
    float overlap      = std::max(0.0f, overlapEnd - overlapStart);

    if (overlap < 0.06)
        return false;

    if (overlap <= 0.001f)
        overlap = std::min(edgeA.length, edgeB.length) * 0.5f;

    float overlapMid = (overlapStart + overlapEnd) * 0.5f;
    glm::vec3 midOnAxis = anchor + seamAxis * overlapMid;

    glm::vec3 pointA =
        closestPointOnSegmentSimple(midOnAxis, edgeA.aWorld, edgeA.bWorld);

    glm::vec3 pointB =
        closestPointOnSegmentSimple(midOnAxis, edgeB.aWorld, edgeB.bWorld);

    float edgeDistance = glm::length(pointA - pointB);
    // DEBUG broad filter: оставляем только пары ребер, которые не совсем в разных галактиках
    if (edgeDistance > 0.38f)
        return false;

    // Если реальные ребра далеко — это не соседний стык,
    // даже если толстые strips случайно пересеклись.
    // if (edgeDistance > 0.38f)
    //     return false;

    int hitCount = 0;

if (stripsIntersect(edgeA.plus,  edgeB.plus))  ++hitCount;
if (stripsIntersect(edgeA.plus,  edgeB.minus)) ++hitCount;
if (stripsIntersect(edgeA.minus, edgeB.plus))  ++hitCount;
if (stripsIntersect(edgeA.minus, edgeB.minus)) ++hitCount;

// ВАЖНО:
// 4 strip'а существуют всегда: 2 от edgeA + 2 от edgeB.
// Но физически не все 4 pairwise-пересечения обязаны произойти.
// Для панелей под углом часто пересекается только рабочая сторона.
// Поэтому strips здесь только подтверждают контакт.
// if (hitCount < 1)
//     return false;

    // Твоё правило: 4 -> seam, 2 -> мусор.
    if (hitCount != 4)
        return false;

    glm::vec3 center = (pointA + pointB) * 0.5f;

    glm::vec3 axisY = pointB - pointA;
    if (glm::length2(axisY) < 1e-8f)
        axisY = edgeA.plus.orientation[1];

    axisY = safeNormalize(axisY, glm::vec3(0,1,0));

    glm::vec3 axisZ =
        safeNormalize(glm::cross(seamAxis, axisY), glm::vec3(0,0,1));

    axisY =
        safeNormalize(glm::cross(axisZ, seamAxis), glm::vec3(0,1,0));

    glm::mat3 orientation(1.0f);
    orientation[0] = seamAxis;
    orientation[1] = axisY;
    orientation[2] = axisZ;

    std::string sortedA = panelA;
    std::string sortedB = panelB;
    if (sortedA > sortedB)
        std::swap(sortedA, sortedB);

    outSeam.id =
        "panel_seam::" +
        sortedA + "::" +
        sortedB + "::" +
        std::to_string(seamIndex);

    outSeam.ownerModuleId = sortedA;
    outSeam.moduleAId = sortedA;
    outSeam.moduleBId = sortedB;
    outSeam.kind = StructuralLinkKind::PanelSeam;

    outSeam.center = center;
    outSeam.halfSize = glm::vec3(
        overlap * 0.5f,
        std::min(0.075f, std::max(0.045f, edgeDistance * 0.5f + 0.025f)),
        0.055f
    );
    outSeam.orientation = orientation;

    outSeam.maxHealth = 100.0f;
    outSeam.health = 100.0f;
    outSeam.impulseTolerance = 250.0f;
    outSeam.loadBearing = true;
    outSeam.destroyed = false;
    outSeam.autoGenerated = true;


    if (seamDebugCounter < 200)
    {
        std::cout
            << "[SEAM DEBUG ACCEPT] "
            << panelA << " <-> " << panelB
            << " axisDot=" << axisDot
            << " overlap=" << overlap
            << " edgeDistance=" << edgeDistance
            << " hitCount=" << hitCount
            << std::endl;

        ++seamDebugCounter;
    }

    return true;
}







static StructuralLinkState makePanelSeamFromStripGroup(
    const std::string& panelA,
    const std::string& panelB,
    const std::vector<const EdgeStrip*>& strips,
    int seamIndex
)
{
    const EdgeStrip* longest = nullptr;

    for (const EdgeStrip* s : strips)
    {
        if (!s)
            continue;

        if (!longest || s->halfSize.x > longest->halfSize.x)
            longest = s;
    }

    glm::vec3 axisX =
        longest
            ? safeNormalize(longest->orientation[0], glm::vec3(1,0,0))
            : glm::vec3(1,0,0);

    glm::vec3 axisY =
        longest
            ? safeNormalize(longest->orientation[1], glm::vec3(0,1,0))
            : glm::vec3(0,1,0);

    glm::vec3 axisZ =
        safeNormalize(glm::cross(axisX, axisY), glm::vec3(0,0,1));

    axisY = safeNormalize(glm::cross(axisZ, axisX), glm::vec3(0,1,0));

    glm::mat3 orientation(1.0f);
    orientation[0] = axisX;
    orientation[1] = axisY;
    orientation[2] = axisZ;

    float minX =  std::numeric_limits<float>::max();
    float minY =  std::numeric_limits<float>::max();
    float minZ =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    auto pushCorner = [&](const glm::vec3& p)
    {
        const float x = glm::dot(p, axisX);
        const float y = glm::dot(p, axisY);
        const float z = glm::dot(p, axisZ);

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);

        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        maxZ = std::max(maxZ, z);
    };

    for (const EdgeStrip* s : strips)
    {
        if (!s)
            continue;

        glm::vec3 sx = s->orientation[0] * s->halfSize.x;
        glm::vec3 sy = s->orientation[1] * s->halfSize.y;
        glm::vec3 sz = s->orientation[2] * s->halfSize.z;

        for (int ix = -1; ix <= 1; ix += 2)
            for (int iy = -1; iy <= 1; iy += 2)
                for (int iz = -1; iz <= 1; iz += 2)
                    pushCorner(s->center + float(ix) * sx + float(iy) * sy + float(iz) * sz);
    }

    glm::vec3 localCenter(
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (minZ + maxZ) * 0.5f
    );

    glm::vec3 center =
        axisX * localCenter.x +
        axisY * localCenter.y +
        axisZ * localCenter.z;

    glm::vec3 halfSize(
        (maxX - minX) * 0.5f,
        (maxY - minY) * 0.5f,
        (maxZ - minZ) * 0.5f
    );

    // Защита от “валунов”: seam должен быть длиннее, чем толстый.
    halfSize.y = std::min(halfSize.y, 0.075f);
    halfSize.z = std::min(halfSize.z, 0.075f);

    std::string sortedA = panelA;
    std::string sortedB = panelB;
    if (sortedA > sortedB)
        std::swap(sortedA, sortedB);

    StructuralLinkState seam;
    seam.id =
        "panel_seam::" +
        sortedA + "::" +
        sortedB + "::" +
        std::to_string(seamIndex);

    seam.ownerModuleId = sortedA;
    seam.moduleAId = sortedA;
    seam.moduleBId = sortedB;
    seam.kind = StructuralLinkKind::PanelSeam;

    seam.center = center;
    seam.halfSize = halfSize;
    seam.orientation = orientation;

    seam.maxHealth = 100.0f;
    seam.health = 100.0f;
    seam.impulseTolerance = 250.0f;
    seam.loadBearing = true;
    seam.destroyed = false;
    seam.autoGenerated = true;

    return seam;
}






static StructuralLinkState makePanelSeamFromStripPair(
    const std::string& panelA,
    const std::string& panelB,
    const EdgeStrip& a,
    const EdgeStrip& b
)
{
    glm::vec3 axisA =
        safeNormalize(a.orientation[0], glm::vec3(1,0,0));

    glm::vec3 axisB =
        safeNormalize(b.orientation[0], axisA);

    if (glm::dot(axisA, axisB) < 0.0f)
        axisB = -axisB;

    glm::vec3 axisX =
        safeNormalize(axisA + axisB, axisA);

    glm::vec3 axisY =
        safeNormalize((a.orientation[1] + b.orientation[1]) * 0.5f, glm::vec3(0,1,0));

    if (std::fabs(glm::dot(axisX, axisY)) > 0.95f)
        axisY = safeNormalize(a.orientation[1], glm::vec3(0,1,0));

    glm::vec3 axisZ =
        safeNormalize(glm::cross(axisX, axisY), glm::vec3(0,0,1));

    axisY =
        safeNormalize(glm::cross(axisZ, axisX), glm::vec3(0,1,0));

    glm::mat3 orientation(1.0f);
    orientation[0] = axisX;
    orientation[1] = axisY;
    orientation[2] = axisZ;

    const glm::vec3 center =
        (a.center + b.center) * 0.5f;

    glm::vec3 halfSize;
    halfSize.x = std::max(a.halfSize.x, b.halfSize.x);
    halfSize.y = std::max(a.halfSize.y, b.halfSize.y);
    halfSize.z = std::max(a.halfSize.z, b.halfSize.z);

    std::string sortedA = panelA;
    std::string sortedB = panelB;
    if (sortedA > sortedB)
        std::swap(sortedA, sortedB);

    StructuralLinkState seam;
    seam.id = "panel_seam::" + sortedA + "::" + sortedB;

    seam.ownerModuleId = sortedA;
    seam.moduleAId = sortedA;
    seam.moduleBId = sortedB;
    seam.kind = StructuralLinkKind::PanelSeam;

    seam.center = center;
    seam.halfSize = halfSize;
    seam.orientation = orientation;

    seam.maxHealth = 100.0f;
    seam.health = 100.0f;
    seam.impulseTolerance = 250.0f;
    seam.loadBearing = true;
    seam.destroyed = false;
    seam.autoGenerated = true;

    return seam;
}




static void expandByTransformedCorners(
    const glm::vec3& localMin,
    const glm::vec3& localMax,
    const glm::mat4& model,
    bool& found,
    glm::vec3& minV,
    glm::vec3& maxV
)
{
    std::array<glm::vec3, 8> corners =
    {{
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    }};

    for (const auto& c : corners)
    {
        glm::vec3 w = glm::vec3(model * glm::vec4(c, 1.0f));

        if (!found)
        {
            minV = w;
            maxV = w;
            found = true;
        }
        else
        {
            minV = glm::min(minV, w);
            maxV = glm::max(maxV, w);
        }
    }
}

static RuntimeVolumeBuildResult buildRuntimeVolumeForDescriptor(
    const ObjectAssembly& assembly,
    const ModuleDescriptor& desc,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    RuntimeVolumeBuildResult result;

    if (desc.meshPartIds.empty())
        return result;

    if (desc.meshPartIds.size() == 1)
    {
        const std::string& wantedId = desc.meshPartIds[0];

        for (const auto& module : assembly.modules)
        {
            glm::mat4 moduleLocalModel =
                world::modules::buildAssemblyModuleHierarchicalLocalModel(
                    assembly,
                    assemblyRuntime,
                    module.id
                );

            for (const auto& part : module.meshes)
            {
                if (part.id != wantedId)
                    continue;

                glm::mat4 partLocalModel =
                    moduleLocalModel * glm::translate(glm::mat4(1.0f), part.localOffset);

                glm::vec3 localCenter =
                    (part.lod0Mesh.minBounds + part.lod0Mesh.maxBounds) * 0.5f;
                glm::vec3 localHalf =
                    (part.lod0Mesh.maxBounds - part.lod0Mesh.minBounds) * 0.5f;

                glm::vec4 transformedCenter =
                    partLocalModel * glm::vec4(localCenter, 1.0f);

                result.valid = true;
                result.isObb = true;
                result.center = glm::vec3(transformedCenter);
                result.halfSize = localHalf;
                result.orientation = glm::mat3(partLocalModel);

                result.orientation[0] = safeNormalize(result.orientation[0], glm::vec3(1.0f, 0.0f, 0.0f));
                result.orientation[1] = safeNormalize(result.orientation[1], glm::vec3(0.0f, 1.0f, 0.0f));
                result.orientation[2] = safeNormalize(result.orientation[2], glm::vec3(0.0f, 0.0f, 1.0f));

                return result;
            }
        }

        return result;
    }

    std::unordered_set<std::string> wanted(
        desc.meshPartIds.begin(),
        desc.meshPartIds.end()
    );

    bool found = false;
    glm::vec3 minV(0.0f);
    glm::vec3 maxV(0.0f);

    for (const auto& module : assembly.modules)
    {
        glm::mat4 moduleLocalModel =
            world::modules::buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                assemblyRuntime,
                module.id
            );

        for (const auto& part : module.meshes)
        {
            if (wanted.find(part.id) == wanted.end())
                continue;

            glm::mat4 partLocalModel =
                moduleLocalModel * glm::translate(glm::mat4(1.0f), part.localOffset);

            expandByTransformedCorners(
                part.lod0Mesh.minBounds,
                part.lod0Mesh.maxBounds,
                partLocalModel,
                found,
                minV,
                maxV
            );
        }
    }

    if (!found)
        return result;

    result.valid = true;
    result.isObb = false;
    result.aabbMin = minV;
    result.aabbMax = maxV;
    result.center = (minV + maxV) * 0.5f;
    result.halfSize = (maxV - minV) * 0.5f;
    result.orientation = glm::mat3(1.0f);

    return result;
}






static float clamp01(float v)
{
    return std::max(0.0f, std::min(1.0f, v));
}

static glm::vec3 closestPointOnSegment(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b
)
{
    glm::vec3 ab = b - a;
    float abLen2 = glm::length2(ab);
    if (abLen2 <= 1e-10f)
        return a;

    float t = glm::dot(p - a, ab) / abLen2;
    t = clamp01(t);
    return a + ab * t;
}

static void closestPointsBetweenSegments(
    const glm::vec3& p1,
    const glm::vec3& q1,
    const glm::vec3& p2,
    const glm::vec3& q2,
    glm::vec3& c1,
    glm::vec3& c2
)
{
    const float EPS = 1e-6f;

    glm::vec3 d1 = q1 - p1;
    glm::vec3 d2 = q2 - p2;
    glm::vec3 r = p1 - p2;

    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= EPS && e <= EPS)
    {
        c1 = p1;
        c2 = p2;
        return;
    }

    if (a <= EPS)
    {
        s = 0.0f;
        t = clamp01(f / e);
    }
    else
    {
        float c = glm::dot(d1, r);

        if (e <= EPS)
        {
            t = 0.0f;
            s = clamp01(-c / a);
        }
        else
        {
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;

            if (std::fabs(denom) > EPS)
                s = clamp01((b * f - c * e) / denom);
            else
                s = 0.0f;

            float tnom = b * s + f;

            if (tnom < 0.0f)
            {
                t = 0.0f;
                s = clamp01(-c / a);
            }
            else if (tnom > e)
            {
                t = 1.0f;
                s = clamp01((b - c) / a);
            }
            else
            {
                t = tnom / e;
            }
        }
    }

    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}





static std::vector<BoundaryEdgeLocal> extractBoundaryEdgesLocal(const MeshData& mesh)
{
    struct EdgeInfo
    {
        int count = 0;
        int triIndex = -1;
        int a = -1;
        int b = -1;
    };

    std::map<std::pair<int, int>, EdgeInfo> edgeMap;

    for (int ti = 0; ti < static_cast<int>(mesh.triangles.size()); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        int ids[3] = { t.v0, t.v1, t.v2 };

        for (int e = 0; e < 3; ++e)
        {
            int a = ids[e];
            int b = ids[(e + 1) % 3];

            std::pair<int, int> key =
                (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);

            auto& info = edgeMap[key];
            info.count += 1;
            info.triIndex = ti;
            info.a = a;
            info.b = b;
        }
    }

    std::vector<BoundaryEdgeLocal> out;

    for (const auto& [key, info] : edgeMap)
    {
        if (info.count != 1)
            continue;

        if (info.a < 0 || info.b < 0)
            continue;

        if (info.a >= static_cast<int>(mesh.vertices.size()) ||
            info.b >= static_cast<int>(mesh.vertices.size()))
            continue;

        const glm::vec3& a = mesh.vertices[info.a].position;
        const glm::vec3& b = mesh.vertices[info.b].position;

        glm::vec3 edgeVec = b - a;
        float len = glm::length(edgeVec);
        if (len <= 1e-5f)
            continue;

        BoundaryEdgeLocal e;
        e.aLocal = a;
        e.bLocal = b;
        e.dirLocal = edgeVec / len;
        e.length = len;

        if (info.triIndex >= 0 && info.triIndex < static_cast<int>(mesh.triangles.size()))
        {
            const auto& tri = mesh.triangles[info.triIndex];
            const glm::vec3& p0 = mesh.vertices[tri.v0].position;
            const glm::vec3& p1 = mesh.vertices[tri.v1].position;
            const glm::vec3& p2 = mesh.vertices[tri.v2].position;

            glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
            e.faceNormalLocal = safeNormalize(n, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        out.push_back(std::move(e));
    }

    return out;
}













static std::vector<BoundaryEdgeLocal> extractFeatureEdgesLocal(const MeshData& mesh)
{
    struct EdgeInfo
    {
        int count = 0;
        int tri0 = -1;
        int tri1 = -1;
        int a = -1;
        int b = -1;
    };

    std::vector<glm::vec3> triNormals;
    triNormals.reserve(mesh.triangles.size());

    for (const auto& t : mesh.triangles)
    {
        const glm::vec3& p0 = mesh.vertices[t.v0].position;
        const glm::vec3& p1 = mesh.vertices[t.v1].position;
        const glm::vec3& p2 = mesh.vertices[t.v2].position;

        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        triNormals.push_back(safeNormalize(n, glm::vec3(0.0f, 0.0f, 1.0f)));
    }

    std::map<std::pair<int, int>, EdgeInfo> edgeMap;

    for (int ti = 0; ti < static_cast<int>(mesh.triangles.size()); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        int ids[3] = { t.v0, t.v1, t.v2 };

        for (int e = 0; e < 3; ++e)
        {
            int a = ids[e];
            int b = ids[(e + 1) % 3];

            std::pair<int, int> key =
                (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);

            auto& info = edgeMap[key];

            if (info.count == 0)
            {
                info.a = a;
                info.b = b;
                info.tri0 = ti;
            }
            else if (info.count == 1)
            {
                info.tri1 = ti;
            }

            info.count += 1;
        }
    }

    std::vector<BoundaryEdgeLocal> out;

    const float featureDotThreshold = 0.92f;
    // dot < 0.92 => угол больше ~23 градусов.
    // Диагонали coplanar face обычно имеют dot около 1 и будут выкинуты.

    for (const auto& [key, info] : edgeMap)
    {
        if (info.a < 0 || info.b < 0)
            continue;

        if (info.a >= static_cast<int>(mesh.vertices.size()) ||
            info.b >= static_cast<int>(mesh.vertices.size()))
            continue;

        bool keep = false;
        glm::vec3 normal(0.0f, 0.0f, 1.0f);

        if (info.count == 1)
        {
            keep = true;
            normal = triNormals[info.tri0];
        }
        else if (info.count == 2)
        {
            const glm::vec3 n0 = triNormals[info.tri0];
            const glm::vec3 n1 = triNormals[info.tri1];

            const float d = std::fabs(glm::dot(n0, n1));

            if (d < featureDotThreshold)
            {
                keep = true;
                normal = safeNormalize(n0 + n1, n0);
            }
        }

        if (!keep)
            continue;

        const glm::vec3& a = mesh.vertices[info.a].position;
        const glm::vec3& b = mesh.vertices[info.b].position;

        glm::vec3 edgeVec = b - a;
        float len = glm::length(edgeVec);
        if (len <= 1e-5f)
            continue;

        BoundaryEdgeLocal e;
        e.aLocal = a;
        e.bLocal = b;
        e.dirLocal = edgeVec / len;
        e.length = len;
        e.faceNormalLocal = safeNormalize(normal, glm::vec3(0.0f, 0.0f, 1.0f));

        out.push_back(std::move(e));
    }

    return out;
}






static std::vector<BoundaryEdgeLocal> extractPanelContourEdgesLocal(const MeshData& mesh)
{
    struct EdgeInfo
    {
        int count = 0;
        int tri0 = -1;
        int tri1 = -1;
        int a = -1;
        int b = -1;
    };

    std::vector<glm::vec3> triNormals;
    triNormals.reserve(mesh.triangles.size());

    for (const auto& t : mesh.triangles)
    {
        const glm::vec3& p0 = mesh.vertices[t.v0].position;
        const glm::vec3& p1 = mesh.vertices[t.v1].position;
        const glm::vec3& p2 = mesh.vertices[t.v2].position;

        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        triNormals.push_back(safeNormalize(n, glm::vec3(0.0f, 0.0f, 1.0f)));
    }

    std::map<std::pair<int, int>, EdgeInfo> edgeMap;

    for (int ti = 0; ti < static_cast<int>(mesh.triangles.size()); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        int ids[3] = { t.v0, t.v1, t.v2 };

        for (int e = 0; e < 3; ++e)
        {
            int a = ids[e];
            int b = ids[(e + 1) % 3];

            std::pair<int, int> key =
                (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);

            auto& info = edgeMap[key];

            if (info.count == 0)
            {
                info.a = a;
                info.b = b;
                info.tri0 = ti;
            }
            else if (info.count == 1)
            {
                info.tri1 = ti;
            }

            ++info.count;
        }
    }

    std::vector<BoundaryEdgeLocal> out;

    const glm::vec3 size = mesh.maxBounds - mesh.minBounds;
    const float maxSize = std::max(size.x, std::max(size.y, size.z));

    const float minContourEdgeLength = std::max(0.20f, maxSize * 0.035f);

    // dot < 0.78 ~= угол больше 38 градусов.
    // Это оставляет резкие кромки: широкая грань -> узкая грань.
    // Почти плоские диагонали одной поверхности вылетают.
    const float hardEdgeDotThreshold = 0.78f;

    for (const auto& [key, info] : edgeMap)
    {
        if (info.a < 0 || info.b < 0)
            continue;

        if (info.a >= static_cast<int>(mesh.vertices.size()) ||
            info.b >= static_cast<int>(mesh.vertices.size()))
            continue;

        bool keep = false;
        glm::vec3 edgeNormal(0.0f, 0.0f, 1.0f);

        if (info.count == 1)
        {
            // Открытая граница меша — оставляем.
            keep = true;
            edgeNormal = triNormals[info.tri0];
        }
        else if (info.count == 2)
        {
            const glm::vec3 n0 = triNormals[info.tri0];
            const glm::vec3 n1 = triNormals[info.tri1];

            const float d = std::fabs(glm::dot(n0, n1));

            if (d < hardEdgeDotThreshold)
            {
                keep = true;

                // Берём среднюю нормаль для смещения strip-валумов.
                // Если нормали противоположные/плохие — fallback на первую.
                glm::vec3 sum = n0 + n1;
                edgeNormal = safeNormalize(sum, n0);
            }
        }

        if (!keep)
            continue;

        const glm::vec3& a = mesh.vertices[info.a].position;
        const glm::vec3& b = mesh.vertices[info.b].position;

        glm::vec3 edgeVec = b - a;
        float len = glm::length(edgeVec);

        if (len < minContourEdgeLength)
            continue;

        BoundaryEdgeLocal e;
        e.aLocal = a;
        e.bLocal = b;
        e.dirLocal = edgeVec / len;
        e.faceNormalLocal = safeNormalize(edgeNormal, glm::vec3(0.0f, 0.0f, 1.0f));
        e.length = len;

        out.push_back(std::move(e));
    }

    return out;
}





static bool isEdgeNearPanelOuterBounds(
    const BoundaryEdgeLocal& edge,
    const MeshData& mesh
)
{
    const glm::vec3 mn = mesh.minBounds;
    const glm::vec3 mx = mesh.maxBounds;

    const glm::vec3 size = mx - mn;

    const glm::vec3 mid = (edge.aLocal + edge.bLocal) * 0.5f;

    const float minSize =
        std::max(0.001f, std::min(size.x, std::min(size.y, size.z)));

    const float maxSize =
        std::max(size.x, std::max(size.y, size.z));

    // Допуск: чуть больше толщины панели, но не бесконечный.
    const float eps =
        std::max(0.08f, std::min(0.25f, minSize * 2.5f));

    int nearFaces = 0;

    if (std::fabs(mid.x - mn.x) <= eps) ++nearFaces;
    if (std::fabs(mid.x - mx.x) <= eps) ++nearFaces;
    if (std::fabs(mid.y - mn.y) <= eps) ++nearFaces;
    if (std::fabs(mid.y - mx.y) <= eps) ++nearFaces;
    if (std::fabs(mid.z - mn.z) <= eps) ++nearFaces;
    if (std::fabs(mid.z - mx.z) <= eps) ++nearFaces;

    // Ребро внешнего периметра обычно лежит около пересечения двух внешних сторон bbox.
    // Внутренние ребра часто будут nearFaces == 0 или 1.
    if (nearFaces >= 2)
        return true;

    // Для очень тонких панелей иногда длинная кромка может давать nearFaces == 1.
    // Тогда оставляем только если ребро длинное и почти на внешней границе.
    if (nearFaces == 1 && edge.length > maxSize * 0.25f)
        return true;
    
    // НОВОЕ: допускаем длинные ребра вообще
    if (edge.length > maxSize * 0.55f)
        return true;

    // Дополнительный фильтр: отбрасываем ребра,
    // которые лежат почти в плоскости панели (внутренние)
    const float normalAlignment =
        std::fabs(glm::dot(
            safeNormalize(edge.faceNormalLocal, glm::vec3(0,0,1)),
            glm::vec3(0,0,1)
        ));

    // если нормаль почти совпадает с осью,
    // это часто внутренняя грань толщины
    if (normalAlignment > 0.95f && edge.length < maxSize * 0.5f)
        return false;

    return false;
}

static std::vector<BoundaryEdgeLocal> filterEdgesNearPanelOuterBounds(
    const std::vector<BoundaryEdgeLocal>& edges,
    const MeshData& mesh
)
{
    std::vector<BoundaryEdgeLocal> out;
    out.reserve(edges.size());

    for (const auto& e : edges)
    {
        if (isEdgeNearPanelOuterBounds(e, mesh))
            out.push_back(e);
    }

    return out;
}















static BoundaryEdgeWorld toWorldEdge(
    const BoundaryEdgeLocal& e,
    const glm::mat4& model
)
{
    glm::vec3 aWorld = glm::vec3(model * glm::vec4(e.aLocal, 1.0f));
    glm::vec3 bWorld = glm::vec3(model * glm::vec4(e.bLocal, 1.0f));

    glm::vec3 dirWorld = safeNormalize(bWorld - aWorld, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat3 basis = glm::mat3(model);
    basis[0] = safeNormalize(basis[0], glm::vec3(1.0f, 0.0f, 0.0f));
    basis[1] = safeNormalize(basis[1], glm::vec3(0.0f, 1.0f, 0.0f));
    basis[2] = safeNormalize(basis[2], glm::vec3(0.0f, 0.0f, 1.0f));

    BoundaryEdgeWorld out;
    out.aWorld = aWorld;
    out.bWorld = bWorld;
    out.dirWorld = dirWorld;
    out.faceNormalWorld = safeNormalize(basis * e.faceNormalLocal, glm::vec3(0.0f, 0.0f, 1.0f));
    out.length = glm::length(bWorld - aWorld);
    return out;
}

static bool findSingleMeshPartRef(
    const ObjectAssembly& assembly,
    const ModuleDescriptor& desc,
    const ObjectAssemblyRuntime& assemblyRuntime,
    PartRef& out
)
{
    if (desc.meshPartIds.size() != 1)
        return false;

    const std::string& wantedId = desc.meshPartIds[0];

    for (const auto& module : assembly.modules)
    {
        glm::mat4 moduleLocalModel =
            world::modules::buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                assemblyRuntime,
                module.id
            );

        for (const auto& part : module.meshes)
        {
            if (part.id != wantedId)
                continue;

            out.module = &module;
            out.part = &part;
            out.partLocalModel =
                moduleLocalModel * glm::translate(glm::mat4(1.0f), part.localOffset);
            out.invPartLocalModel = glm::inverse(out.partLocalModel);
            return true;
        }
    }

    return false;
}

static float intervalOverlapLength(
    float a0,
    float a1,
    float b0,
    float b1
)
{
    float minA = std::min(a0, a1);
    float maxA = std::max(a0, a1);
    float minB = std::min(b0, b1);
    float maxB = std::max(b0, b1);

    float start = std::max(minA, minB);
    float end   = std::min(maxA, maxB);

    return std::max(0.0f, end - start);
}




static std::vector<LocalSupportProxy> buildLocalSeamProxyVolumes(
    const ObjectAssembly& assembly,
    const IObjectDescriptor& descriptor,
    const ModuleDescriptor& desc,
    const ObjectAssemblyRuntime& assemblyRuntime,
    std::size_t maxProxyCount
)
{
    std::vector<LocalSupportProxy> out;

    if (desc.structuralKind != ModuleStructuralKind::OuterPanel)
        return out;

    if (maxProxyCount == 0)
        return out;

    PartRef panelA;
    if (!findSingleMeshPartRef(assembly, desc, assemblyRuntime, panelA))
        return out;

    const auto edgesALocal = extractBoundaryEdgesLocal(panelA.part->lod0Mesh);
    if (edgesALocal.empty())
        return out;

    struct RawCandidate
    {
        glm::vec3 centerWorld {0.0f};
        glm::vec3 halfSize {0.05f, 0.05f, 0.05f};
        glm::mat3 orientationWorld {1.0f};
        std::string neighborModuleId;
        float score = 0.0f;
    };

    const float minEdgeLength = 0.08f;
    const float broadPhaseExtra = 0.55f;
    const float overlapMin = 0.02f;
    const float pairDistanceThreshold = 0.35f;
    const float parallelThreshold = 0.65f;

    const float seamHalfWidth = 0.070f;
    const float seamHalfThickness = 0.055f;

    std::vector<RawCandidate> rawCandidates;

    const glm::vec3 centerAWorld =
        glm::vec3(panelA.partLocalModel * glm::vec4(panelA.part->lod0Mesh.boundCenter, 1.0f));
    const float radiusA = panelA.part->lod0Mesh.boundRadius;

    for (const auto& otherDesc : descriptor.moduleDescriptors())
    {
        if (otherDesc.moduleId == desc.moduleId)
            continue;

        if (!otherDesc.enabled)
            continue;

        if (otherDesc.structuralKind != ModuleStructuralKind::OuterPanel)
            continue;

        if (otherDesc.meshPartIds.size() != 1)
            continue;

        PartRef panelB;
        if (!findSingleMeshPartRef(assembly, otherDesc, assemblyRuntime, panelB))
            continue;

        const glm::vec3 centerBWorld =
            glm::vec3(panelB.partLocalModel * glm::vec4(panelB.part->lod0Mesh.boundCenter, 1.0f));
        const float radiusB = panelB.part->lod0Mesh.boundRadius;

        if (glm::distance(centerAWorld, centerBWorld) > (radiusA + radiusB + broadPhaseExtra))
            continue;

        const auto edgesBLocal = extractBoundaryEdgesLocal(panelB.part->lod0Mesh);
        if (edgesBLocal.empty())
            continue;

        bool foundBest = false;
        RawCandidate bestCandidate;

        for (const auto& edgeALocal : edgesALocal)
        {
            if (edgeALocal.length < minEdgeLength)
                continue;

            BoundaryEdgeWorld edgeA = toWorldEdge(edgeALocal, panelA.partLocalModel);

            for (const auto& edgeBLocal : edgesBLocal)
            {
                if (edgeBLocal.length < minEdgeLength)
                    continue;

                BoundaryEdgeWorld edgeB = toWorldEdge(edgeBLocal, panelB.partLocalModel);

                float dirDot = glm::dot(edgeA.dirWorld, edgeB.dirWorld);
                float absDirDot = std::fabs(dirDot);
                if (absDirDot < parallelThreshold)
                    continue;

                glm::vec3 seamAxis =
                    (dirDot >= 0.0f)
                        ? safeNormalize(edgeA.dirWorld + edgeB.dirWorld, edgeA.dirWorld)
                        : safeNormalize(edgeA.dirWorld - edgeB.dirWorld, edgeA.dirWorld);

                glm::vec3 anchor =
                    (edgeA.aWorld + edgeA.bWorld + edgeB.aWorld + edgeB.bWorld) * 0.25f;

                float a0 = glm::dot(edgeA.aWorld - anchor, seamAxis);
                float a1 = glm::dot(edgeA.bWorld - anchor, seamAxis);
                float b0 = glm::dot(edgeB.aWorld - anchor, seamAxis);
                float b1 = glm::dot(edgeB.bWorld - anchor, seamAxis);

                float overlap = intervalOverlapLength(a0, a1, b0, b1);
                if (overlap < overlapMin)
                    continue;

                glm::vec3 closestA(0.0f), closestB(0.0f);
                closestPointsBetweenSegments(
                    edgeA.aWorld, edgeA.bWorld,
                    edgeB.aWorld, edgeB.bWorld,
                    closestA, closestB
                );

                float pairDistance = glm::distance(closestA, closestB);
                if (pairDistance > pairDistanceThreshold)
                    continue;

                float minA = std::min(a0, a1);
                float maxA = std::max(a0, a1);
                float minB = std::min(b0, b1);
                float maxB = std::max(b0, b1);

                float overlapStart = std::max(minA, minB);
                float overlapEnd   = std::min(maxA, maxB);
                float overlapMid   = (overlapStart + overlapEnd) * 0.5f;

                glm::vec3 axisMidPoint = anchor + seamAxis * overlapMid;

                glm::vec3 seamPointA =
                    closestPointOnSegment(axisMidPoint, edgeA.aWorld, edgeA.bWorld);
                glm::vec3 seamPointB =
                    closestPointOnSegment(axisMidPoint, edgeB.aWorld, edgeB.bWorld);

                glm::vec3 centerWorld = (seamPointA + seamPointB) * 0.5f;

                glm::vec3 lateralAxis = seamPointB - seamPointA;
                if (glm::length2(lateralAxis) <= 1e-10f)
                {
                    glm::vec3 avgNormal =
                        safeNormalize(edgeA.faceNormalWorld + edgeB.faceNormalWorld,
                                      glm::vec3(0.0f, 0.0f, 1.0f));

                    lateralAxis = glm::cross(avgNormal, seamAxis);
                }
                lateralAxis = safeNormalize(lateralAxis, glm::vec3(0.0f, 1.0f, 0.0f));

                glm::vec3 normalAxis = glm::cross(seamAxis, lateralAxis);
                if (glm::length2(normalAxis) <= 1e-10f)
                {
                    normalAxis =
                        safeNormalize(edgeA.faceNormalWorld + edgeB.faceNormalWorld,
                                      glm::vec3(0.0f, 0.0f, 1.0f));
                }
                normalAxis = safeNormalize(normalAxis, glm::vec3(0.0f, 0.0f, 1.0f));

                lateralAxis =
                    safeNormalize(glm::cross(normalAxis, seamAxis), glm::vec3(0.0f, 1.0f, 0.0f));

                RawCandidate c;
                c.centerWorld = centerWorld;
                c.halfSize = glm::vec3(
                    overlap * 0.5f,
                    std::max(seamHalfWidth, pairDistance * 0.5f + 0.03f),
                    seamHalfThickness
                );
                c.orientationWorld[0] = seamAxis;
                c.orientationWorld[1] = lateralAxis;
                c.orientationWorld[2] = normalAxis;
                c.neighborModuleId = otherDesc.moduleId;
                c.score = overlap * 10.0f + absDirDot * 2.0f - pairDistance * 4.0f;
                
                rawCandidates.push_back(std::move(c));
                
            }
        }

        
    }

    if (rawCandidates.empty())
    {
        if (debug::get().render.captureSeamDebug)
            debug::get().render.seamDebugProxies.clear();

        return out;
    }

    std::sort(
        rawCandidates.begin(),
        rawCandidates.end(),
        [](const RawCandidate& a, const RawCandidate& b)
        {
            if (a.score != b.score)
                return a.score > b.score;

            if (a.centerWorld.x != b.centerWorld.x) return a.centerWorld.x < b.centerWorld.x;
            if (a.centerWorld.y != b.centerWorld.y) return a.centerWorld.y < b.centerWorld.y;
            return a.centerWorld.z < b.centerWorld.z;
        }
    );

    std::vector<RawCandidate> uniqueCandidates;
    uniqueCandidates.reserve(rawCandidates.size());

    for (const auto& c : rawCandidates)
    {
        bool duplicate = false;

        for (const auto& u : uniqueCandidates)
        {
        

            float centerDist = glm::distance(c.centerWorld, u.centerWorld);
            float axisDot = std::fabs(glm::dot(c.orientationWorld[0], u.orientationWorld[0]));

            if (centerDist < 0.15f && axisDot > 0.95f)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
            uniqueCandidates.push_back(c);

        if (uniqueCandidates.size() >= maxProxyCount)
            break;
    }

    glm::mat3 panelBasis = glm::mat3(panelA.partLocalModel);
    panelBasis[0] = safeNormalize(panelBasis[0], glm::vec3(1.0f, 0.0f, 0.0f));
    panelBasis[1] = safeNormalize(panelBasis[1], glm::vec3(0.0f, 1.0f, 0.0f));
    panelBasis[2] = safeNormalize(panelBasis[2], glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat3 invPanelBasis = glm::transpose(panelBasis);

    for (const auto& c : uniqueCandidates)
    {
        LocalSupportProxy p;
        p.supportModuleId = c.neighborModuleId;
        p.localCenter = glm::vec3(panelA.invPartLocalModel * glm::vec4(c.centerWorld, 1.0f));
        p.halfSize = c.halfSize;

        p.localOrientation[0] =
            safeNormalize(invPanelBasis * c.orientationWorld[0], glm::vec3(1.0f, 0.0f, 0.0f));
        p.localOrientation[1] =
            safeNormalize(invPanelBasis * c.orientationWorld[1], glm::vec3(0.0f, 1.0f, 0.0f));
        p.localOrientation[2] =
            safeNormalize(invPanelBasis * c.orientationWorld[2], glm::vec3(0.0f, 0.0f, 1.0f));

        p.score = c.score;
        out.push_back(std::move(p));
    }

    if (debug::get().render.captureSeamDebug)
    {
        debug::get().render.seamDebugProxies.clear();

        for (const auto& c : uniqueCandidates)
        {
            debug::DebugSeamProxy dbg;
            dbg.centerWorld = c.centerWorld;
            dbg.halfSize = c.halfSize;
            dbg.orientationWorld = c.orientationWorld;
            dbg.score = c.score;
            dbg.neighborModuleId = c.neighborModuleId;

            debug::get().render.seamDebugProxies.push_back(dbg);
        }
    }

    return out;
}


static void appendLinkHitVolumesFromRuntime(
    HitComponent& hitComponent,
    const IObjectDescriptor& descriptor,
    const ObjectStructuralLinkRuntime& structuralLinkRuntime,
    const std::unordered_map<std::string, HitVolume>& oldByKey
)
{
    std::unordered_map<std::string, game::damage::HitZoneType> moduleZoneById;
    std::unordered_map<std::string, int> moduleLayerById;
    std::unordered_map<std::string, int> modulePriorityById;
    std::unordered_map<std::string, std::string> moduleSubsystemById;

    for (const auto& modDesc : descriptor.moduleDescriptors())
    {
        moduleZoneById[modDesc.moduleId] = modDesc.zone;
        moduleLayerById[modDesc.moduleId] = modDesc.layerIndex;
        modulePriorityById[modDesc.moduleId] = modDesc.hitPriority;
        moduleSubsystemById[modDesc.moduleId] = modDesc.subsystemId;
    }

    if (debug::get().render.captureSeamDebug)
        debug::get().render.seamDebugProxies.clear();

    for (const auto& link : structuralLinkRuntime.links())
    {
        HitVolume sv;

        sv.zone = static_cast<game::damage::HitZoneType>(0);

        auto itZone = moduleZoneById.find(link.moduleAId);
        if (itZone != moduleZoneById.end())
            sv.zone = itZone->second;

        sv.priority = 50;

        auto itPriority = modulePriorityById.find(link.moduleAId);
        if (itPriority != modulePriorityById.end())
            sv.priority = itPriority->second + 50;

        sv.layerIndex = 0;

        auto itLayer = moduleLayerById.find(link.moduleAId);
        if (itLayer != moduleLayerById.end())
            sv.layerIndex = itLayer->second;

        sv.center = link.center;
        sv.halfSize = link.halfSize;
        sv.orientation = link.orientation;

        sv.m_label = link.id;
        sv.moduleId = link.moduleAId;

        auto itSubsystem = moduleSubsystemById.find(link.moduleAId);
        sv.subsystemId =
            (itSubsystem != moduleSubsystemById.end())
                ? itSubsystem->second
                : "";

        sv.destructible = true;
        sv.health = link.health;
        sv.maxHealth = link.maxHealth;
        sv.armor = 0.0f;
        sv.penetrationResistance = 0.0f;
        sv.destroyed = link.destroyed;

        sv.supportLinkVolume = true;
        sv.supportLinkId = link.id;
        sv.supportModuleId = link.moduleBId;
        sv.impulseTolerance = link.impulseTolerance;

        auto itOld = oldByKey.find("link::" + link.id);
        if (itOld != oldByKey.end())
        {
            sv.health = link.health;
            sv.maxHealth = link.maxHealth;
            sv.destroyed = link.destroyed || itOld->second.destroyed;
        }

        hitComponent.volumes.push_back(sv);

        if (debug::get().render.captureSeamDebug &&
            link.kind == StructuralLinkKind::PanelSeam)
        {
            debug::DebugSeamProxy dbg;
            dbg.centerWorld = link.center;
            dbg.halfSize = link.halfSize;
            dbg.orientationWorld = link.orientation;
            dbg.score = 1.0f;
            dbg.neighborModuleId = link.moduleBId;

            debug::get().render.seamDebugProxies.push_back(dbg);
        }
    }
}




static CachedHitTemplate buildHitTemplateForType(
    ObjectType typeId,
    const IObjectDescriptor& descriptor,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    CachedHitTemplate result;

    if (!AssemblyMeshLibrary::has(typeId))
        return result;

    const auto& assembly = AssemblyMeshLibrary::get(typeId);

    // ==================================================
    // 1. Шаблон обычных module hit-volumes
    // ==================================================
    for (const auto& modDesc : descriptor.moduleDescriptors())
    {
        if (!modDesc.enabled)
            continue;

        RuntimeVolumeBuildResult built =
            buildRuntimeVolumeForDescriptor(assembly, modDesc, assemblyRuntime);

        if (!built.valid)
            continue;

        HitVolume v;
        v.zone = modDesc.zone;
        v.priority = modDesc.hitPriority;
        v.layerIndex = modDesc.layerIndex;
        v.center = built.center;
        v.halfSize = built.halfSize;
        v.orientation = built.orientation;
        v.m_label = modDesc.moduleId;
        v.moduleId = modDesc.moduleId;
        v.subsystemId = modDesc.subsystemId;
        v.destructible = modDesc.destructible;
        v.health = 100.0f;
        v.maxHealth = 100.0f;
        v.armor = modDesc.armor;
        v.penetrationResistance = modDesc.penetrationResistance;
        v.destroyed = false;

        v.supportLinkVolume = false;

        result.moduleVolumes.push_back(v);
    }



// ==================================================
// 2. Edge strip pairs: по 2 колбаски на каждое ребро
// ==================================================

std::unordered_map<std::string, std::vector<EdgeStripPair>> panelEdgePairs;

const float seamHalfWidth = 0.070f;
const float seamHalfThickness = 0.055f;
const float edgeExtension = 0.08f;
const float minEdgeLength = 0.15f;

for (const auto& modDesc : descriptor.moduleDescriptors())
{
    if (!modDesc.enabled)
        continue;

    if (modDesc.structuralKind != ModuleStructuralKind::OuterPanel)
        continue;

    if (modDesc.meshPartIds.size() != 1)
        continue;

    PartRef partRef;
    if (!findSingleMeshPartRef(assembly, modDesc, assemblyRuntime, partRef))
        continue;

    auto localEdges =
    extractPanelContourEdgesLocal(partRef.part->lod0Mesh);  


    std::cout
    << "[SEAM CONTOUR EDGES] "
    << modDesc.moduleId
    << " edges=" << localEdges.size()
    << std::endl;

    int edgeIndex = 0;

    for (const auto& localEdge : localEdges)
    {
        if (localEdge.length < minEdgeLength)
            continue;

        BoundaryEdgeWorld edgeWorld =
            toWorldEdge(localEdge, partRef.partLocalModel);

        glm::vec3 axisX =
            safeNormalize(edgeWorld.dirWorld, glm::vec3(1,0,0));

        glm::vec3 axisZ =
            safeNormalize(edgeWorld.faceNormalWorld, glm::vec3(0,0,1));

        glm::vec3 axisY =
            safeNormalize(glm::cross(axisZ, axisX), glm::vec3(0,1,0));

        axisZ =
            safeNormalize(glm::cross(axisX, axisY), glm::vec3(0,0,1));

        glm::vec3 mid =
            (edgeWorld.aWorld + edgeWorld.bWorld) * 0.5f;

        glm::vec3 halfSize(
            edgeWorld.length * 0.5f + edgeExtension,
            seamHalfWidth,
            seamHalfThickness
        );

        glm::mat3 orientation(1.0f);
        orientation[0] = axisX;
        orientation[1] = axisY;
        orientation[2] = axisZ;

        EdgeStripPair pair;
        pair.panelId = modDesc.moduleId;
        pair.edgeIndex = edgeIndex;

        pair.aWorld = edgeWorld.aWorld;
        pair.bWorld = edgeWorld.bWorld;
        pair.axisWorld = axisX;
        pair.length = edgeWorld.length;

        pair.plus.panelId = modDesc.moduleId;
        pair.plus.center = mid + axisZ * seamHalfThickness;
        pair.plus.halfSize = halfSize;
        pair.plus.orientation = orientation;

        glm::mat3 inverseOrientation = orientation;
        inverseOrientation[2] = -axisZ;

        pair.minus.panelId = modDesc.moduleId;
        pair.minus.center = mid - axisZ * seamHalfThickness;
        pair.minus.halfSize = halfSize;
        pair.minus.orientation = inverseOrientation;

        panelEdgePairs[modDesc.moduleId].push_back(std::move(pair));

        ++edgeIndex;
    }
}

// ==================================================
// 3. Формирование финальных seam links
//    edgePair A vs edgePair B:
//    реальные ребра близко + overlap + 4 strips -> 1 seam
// ==================================================

int seamCounter = 0;
std::vector<StructuralLinkState> seamCandidates;

for (auto itA = panelEdgePairs.begin(); itA != panelEdgePairs.end(); ++itA)
{
    auto itB = itA;
    ++itB;

    for (; itB != panelEdgePairs.end(); ++itB)
    {
        const std::string& panelA = itA->first;
        const std::string& panelB = itB->first;

        const auto& pairsA = itA->second;
        const auto& pairsB = itB->second;

        int localSeamIndex = 0;

        for (const auto& edgePairA : pairsA)
        {
            for (const auto& edgePairB : pairsB)
            {
                StructuralLinkState seam;

                if (!tryBuildSeamFromEdgePairs(
                        panelA,
                        panelB,
                        edgePairA,
                        edgePairB,
                        localSeamIndex,
                        seam))
                {
                    continue;
                }

                seamCandidates.push_back(std::move(seam));

                ++localSeamIndex;
                ++seamCounter;
            }
        }
    }
}
auto seamLength = [](const StructuralLinkState& s) -> float
{
    return s.halfSize.x * 2.0f;
};

auto sameSeamDuplicate = [](const StructuralLinkState& a, const StructuralLinkState& b) -> bool
{
    if (a.moduleAId != b.moduleAId || a.moduleBId != b.moduleBId)
        return false;

    glm::vec3 axisA = safeNormalize(a.orientation[0], glm::vec3(1,0,0));
    glm::vec3 axisB = safeNormalize(b.orientation[0], glm::vec3(1,0,0));

    float axisDot = std::fabs(glm::dot(axisA, axisB));
    if (axisDot < 0.97f)
        return false;

    float centerDist = glm::distance(a.center, b.center);
    if (centerDist > 0.22f)
        return false;

    return true;
};

std::sort(
    seamCandidates.begin(),
    seamCandidates.end(),
    [&](const StructuralLinkState& a, const StructuralLinkState& b)
    {
        return seamLength(a) > seamLength(b);
    }
);

std::vector<StructuralLinkState> uniqueSeams;
uniqueSeams.reserve(seamCandidates.size());

for (const auto& s : seamCandidates)
{
    bool duplicate = false;

    for (const auto& u : uniqueSeams)
    {
        if (sameSeamDuplicate(s, u))
        {
            duplicate = true;
            break;
        }
    }

    if (!duplicate)
        uniqueSeams.push_back(s);
}

for (auto& s : uniqueSeams)
{
    result.structuralLinks.push_back(std::move(s));
}

seamCounter = static_cast<int>(uniqueSeams.size());

std::cout
    << "[SEAM DEDUP] candidates=" << seamCandidates.size()
    << " unique=" << uniqueSeams.size()
    << std::endl;
std::cout << "[SEAM FINAL] count=" << seamCounter << std::endl;




    std::cout
        << "[HIT TEMPLATE BUILD] typeId=" << static_cast<int>(typeId)
        << " moduleVolumes=" << result.moduleVolumes.size()
        << " structuralLinks=" << result.structuralLinks.size()
        << std::endl;

    return result;
}






static const CachedHitTemplate& getOrBuildHitTemplateForType(
    ObjectType typeId,
    const IObjectDescriptor& descriptor,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    const int key = makeHitTemplateCacheKey(typeId);

    auto it = s_hitTemplateCache.find(key);
    if (it != s_hitTemplateCache.end())
        return it->second;

    CachedHitTemplate built =
        buildHitTemplateForType(typeId, descriptor, assemblyRuntime);

    auto [insertedIt, _] =
        s_hitTemplateCache.emplace(key, std::move(built));

    return insertedIt->second;
}






void ObjectRuntimeHitBuilder::rebuild(
    HitComponent& hitComponent,
    ObjectType typeId,
    const IObjectDescriptor& descriptor,
    const ObjectModuleRuntime& moduleRuntime,
    ObjectStructuralLinkRuntime& structuralLinkRuntime,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    if (!AssemblyMeshLibrary::has(typeId))
        return;

    static int rebuildCounter = 0;
    ++rebuildCounter;

    if (rebuildCounter <= 10)
    {
        std::cout
            << "[HIT REBUILD] count=" << rebuildCounter
            << " typeId=" << static_cast<int>(typeId)
            << std::endl;
    }

    const CachedHitTemplate& cachedTemplate =
        getOrBuildHitTemplateForType(typeId, descriptor, assemblyRuntime);

    std::unordered_map<std::string, HitVolume> oldByKey;
    for (const auto& oldV : hitComponent.volumes)
    {
        const std::string key =
            oldV.supportLinkVolume
                ? ("link::" + oldV.supportLinkId)
                : ("module::" + oldV.moduleId);

        oldByKey[key] = oldV;
    }

    std::unordered_map<std::string, StructuralLinkState> oldAutoSeams;
    for (const auto& link : structuralLinkRuntime.links())
    {
        if (link.autoGenerated && link.kind == StructuralLinkKind::PanelSeam)
            oldAutoSeams[link.id] = link;
    }

    hitComponent.volumes.clear();

    // ==================================================
    // 1. Module volumes из кешированного шаблона + runtime state
    // ==================================================
    for (const auto& templatedVolume : cachedTemplate.moduleVolumes)
    {
        if (!moduleRuntime.moduleParticipatesInHits(templatedVolume.moduleId))
            continue;

        const ObjectModuleState* runtimeModule =
            moduleRuntime.findModule(templatedVolume.moduleId);

        if (!runtimeModule)
            continue;

        HitVolume v = templatedVolume;

        v.health = runtimeModule->health;
        v.maxHealth = runtimeModule->maxHealth;
        v.destroyed = runtimeModule->isDestroyedLike();

        auto itOld = oldByKey.find("module::" + v.moduleId);
        if (itOld != oldByKey.end())
        {
            v.health = runtimeModule->health;
            v.maxHealth = runtimeModule->maxHealth;
            v.destroyed = runtimeModule->isDestroyedLike() || itOld->second.destroyed;
        }

        hitComponent.volumes.push_back(v);
    }

    // ==================================================
    // 2. Structural links из кешированного шаблона + сохранение runtime state
    // ==================================================
    structuralLinkRuntime.clearAutoGenerated(StructuralLinkKind::PanelSeam);

    for (auto link : cachedTemplate.structuralLinks)
    {
        auto itOld = oldAutoSeams.find(link.id);
        if (itOld != oldAutoSeams.end())
        {
            link.health = itOld->second.health;
            link.maxHealth = itOld->second.maxHealth;
            link.impulseTolerance = itOld->second.impulseTolerance;
            link.destroyed = itOld->second.destroyed;
        }

        structuralLinkRuntime.addOrReplaceAutoGenerated(link);
    }

    // ==================================================
    // 3. Финальные HitVolume из current structuralLinkRuntime
    // ==================================================
    appendLinkHitVolumesFromRuntime(
        hitComponent,
        descriptor,
        structuralLinkRuntime,
        oldByKey
    );
}



} // namespace world::modules