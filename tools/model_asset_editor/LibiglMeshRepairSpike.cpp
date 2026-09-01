#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <igl/embree/reorient_facets_raycast.h>
#include <igl/is_edge_manifold.h>
#include <igl/is_vertex_manifold.h>
#include <igl/read_triangle_mesh.h>
#include <igl/remove_duplicate_vertices.h>
#include <igl/remove_unreferenced.h>
#include <igl/split_nonmanifold.h>
#include <igl/unique_simplices.h>
#include <igl/writeOBJ.h>

namespace
{
constexpr double kWeldEpsilon = 1.0e-4;

int patchCount(const Eigen::VectorXi& component)
{
    if (component.size() == 0) return 0;
    return component.maxCoeff() + 1;
}

void printState(
    const char* label,
    const Eigen::MatrixXd& vertices,
    const Eigen::MatrixXi& faces)
{
    std::cout
        << label
        << " vertices=" << vertices.rows()
        << " triangles=" << faces.rows()
        << " edge_manifold=" << (igl::is_edge_manifold(faces) ? "yes" : "no")
        << " vertex_manifold=" << (igl::is_vertex_manifold(faces) ? "yes" : "no")
        << '\n';
}

Eigen::MatrixXi removeCollapsedFaces(
    const Eigen::MatrixXd& vertices,
    const Eigen::MatrixXi& faces,
    int& removed)
{
    std::vector<int> keep;
    keep.reserve(static_cast<std::size_t>(faces.rows()));

    const Eigen::RowVector3d extent = vertices.colwise().maxCoeff() - vertices.colwise().minCoeff();
    const double scale = std::max(1.0, extent.squaredNorm());
    const double minCrossSquared = scale * scale * 1.0e-24;

    for (int f = 0; f < faces.rows(); ++f)
    {
        const int a = faces(f, 0);
        const int b = faces(f, 1);
        const int c = faces(f, 2);
        if (a == b || b == c || c == a)
            continue;

        const Eigen::Vector3d ab = (vertices.row(b) - vertices.row(a)).transpose();
        const Eigen::Vector3d ac = (vertices.row(c) - vertices.row(a)).transpose();
        if (ab.cross(ac).squaredNorm() <= minCrossSquared)
            continue;

        keep.push_back(f);
    }

    removed = faces.rows() - static_cast<int>(keep.size());
    Eigen::MatrixXi result(static_cast<int>(keep.size()), 3);
    for (int i = 0; i < static_cast<int>(keep.size()); ++i)
        result.row(i) = faces.row(keep[static_cast<std::size_t>(i)]);
    return result;
}

Eigen::MatrixXi removeDuplicateFacesPreservingWinding(
    const Eigen::MatrixXi& faces,
    int& removed)
{
    Eigen::MatrixXi sortedUnique;
    Eigen::VectorXi representative;
    Eigen::VectorXi inverse;
    igl::unique_simplices(faces, sortedUnique, representative, inverse);

    Eigen::MatrixXi result(representative.rows(), 3);
    for (int i = 0; i < representative.rows(); ++i)
        result.row(i) = faces.row(representative(i));

    removed = faces.rows() - result.rows();
    return result;
}

Eigen::MatrixXi applyFlips(
    const Eigen::MatrixXi& faces,
    const Eigen::VectorXi& flips)
{
    Eigen::MatrixXi result = faces;
    for (int f = 0; f < result.rows(); ++f)
    {
        if (flips(f) != 0)
            std::swap(result(f, 0), result(f, 2));
    }
    return result;
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr
            << "Usage: model_asset_libigl_spike <input.obj> [output.obj]\n"
            << "\n"
            << "Stage B performs geometry-only libigl preparation:\n"
            << "  read -> weld -> degenerate/duplicate cleanup -> split_nonmanifold\n"
            << "       -> Embree reorient_facets_raycast -> write OBJ\n";
        return 2;
    }

    const std::filesystem::path input = std::filesystem::u8path(argv[1]);
    const std::filesystem::path output = argc == 3
        ? std::filesystem::u8path(argv[2])
        : std::filesystem::path(ELITE_SOURCE_ROOT) / "build" / "tools" /
            "model_asset_editor" / "diagnostics" / "libigl" /
            (input.stem().u8string() + "_libigl_raycast.obj");

    Eigen::MatrixXd V;
    Eigen::MatrixXi F;
    if (!igl::read_triangle_mesh(input.u8string(), V, F))
    {
        std::cerr << "LIBIGL_SPIKE FAIL: cannot read " << input.u8string() << '\n';
        return 3;
    }
    if (V.cols() != 3 || F.cols() != 3 || V.rows() == 0 || F.rows() == 0)
    {
        std::cerr
            << "LIBIGL_SPIKE FAIL: expected a non-empty triangle mesh, got V="
            << V.rows() << 'x' << V.cols()
            << " F=" << F.rows() << 'x' << F.cols() << '\n';
        return 4;
    }

    printState("RAW", V, F);

    // OBJ render vertices contain UV/normal seams. For geometric topology they
    // must first be reduced to position identity; split_nonmanifold will split
    // coincident points back apart where manifold/orientable topology requires it.
    Eigen::MatrixXd WV;
    Eigen::MatrixXi WF;
    Eigen::VectorXi WVI;
    Eigen::VectorXi WVJ;
    igl::remove_duplicate_vertices(V, F, kWeldEpsilon, WV, WVI, WVJ, WF);
    printState("WELDED", WV, WF);

    int removedCollapsed = 0;
    Eigen::MatrixXi CF = removeCollapsedFaces(WV, WF, removedCollapsed);

    int removedDuplicates = 0;
    Eigen::MatrixXi UF = removeDuplicateFacesPreservingWinding(CF, removedDuplicates);

    Eigen::MatrixXd CV;
    Eigen::MatrixXi RF;
    Eigen::VectorXi oldToNew;
    Eigen::VectorXi newToOld;
    igl::remove_unreferenced(WV, UF, CV, RF, oldToNew, newToOld);
    printState("CLEAN", CV, RF);

    Eigen::MatrixXd SV;
    Eigen::MatrixXi SF;
    Eigen::VectorXi SVI;
    igl::split_nonmanifold(CV, RF, SV, SF, SVI);
    printState("SPLIT", SV, SF);

    // Do not use libigl's default F.rows()*100 rays for the first real-station
    // spike: ~9M rays is unnecessary to answer whether the method works here.
    const int raysTotal = std::clamp<int>(SF.rows() * 8, 200000, 1000000);
    const int raysMinimum = 16;
    const bool facetWise = false;
    const bool useParity = false;
    const bool verbose = true;

    Eigen::VectorXi flips;
    Eigen::VectorXi components;
    std::cout
        << "RAYCAST rays_total=" << raysTotal
        << " rays_minimum=" << raysMinimum << '\n';
    igl::embree::reorient_facets_raycast(
        SV,
        SF,
        raysTotal,
        raysMinimum,
        facetWise,
        useParity,
        verbose,
        flips,
        components);

    const Eigen::MatrixXi OF = applyFlips(SF, flips);
    printState("RAYCAST_ORIENTED", SV, OF);

    std::error_code ec;
    if (!output.parent_path().empty())
        std::filesystem::create_directories(output.parent_path(), ec);
    if (!output.parent_path().empty() && ec)
    {
        std::cerr
            << "LIBIGL_SPIKE FAIL: cannot create output directory "
            << output.parent_path().u8string() << ": " << ec.message() << '\n';
        return 5;
    }

    if (!igl::writeOBJ(output.u8string(), SV, OF))
    {
        std::cerr << "LIBIGL_SPIKE FAIL: cannot write " << output.u8string() << '\n';
        return 6;
    }

    std::cout
        << "WELDED_VERTICES=" << WV.rows() << '\n'
        << "REMOVED_COLLAPSED_TRIANGLES=" << removedCollapsed << '\n'
        << "REMOVED_DUPLICATE_TRIANGLES=" << removedDuplicates << '\n'
        << "DUPLICATED_TOPOLOGY_VERTICES=" << (SV.rows() - CV.rows()) << '\n'
        << "RAYCAST_PATCHES=" << patchCount(components) << '\n'
        << "RAYCAST_FLIPPED_TRIANGLES=" << flips.cast<int>().sum() << '\n'
        << "OUTPUT=" << output.u8string() << '\n'
        << "LIBIGL_SPIKE PASS\n";
    return 0;
}
