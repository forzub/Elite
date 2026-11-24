// The route-draft net's model contract (audit 8e3c49007 NET-1): a model file that does not match the code's feature
// layout / shapes is REFUSED at load (the planner then runs the generic search) - never read past a buffer, never a
// silently wrong draft. Corrupted copies of a small valid model are written next to the test binary.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/game/navigation/planner/RouteDraftNet.h"

namespace
{
int g_checks = 0, g_failures = 0;
void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

nlohmann::json validModel()
{
    using game::navigation::planner::RouteDraftFeatureVersion;
    using game::navigation::planner::RouteDraftInputs;
    using game::navigation::planner::RouteDraftOutputs;
    nlohmann::json j;
    j["schema"] = "elite.route_draft_net.v1";
    j["feature_version"] = RouteDraftFeatureVersion;
    j["inputs"] = RouteDraftInputs;
    j["outputs"] = RouteDraftOutputs;
    const auto layer = [](int in, int out) {
        nlohmann::json l;
        std::vector<std::vector<float>> w(static_cast<std::size_t>(out), std::vector<float>(static_cast<std::size_t>(in), 0.01f));
        l["w"] = w;
        l["b"] = std::vector<float>(static_cast<std::size_t>(out), 0.0f);
        return l;
    };
    j["layers"] = nlohmann::json::array({layer(RouteDraftInputs, 8), layer(8, RouteDraftOutputs)});
    j["in_mean"] = std::vector<float>(static_cast<std::size_t>(RouteDraftInputs), 0.0f);
    j["in_std"] = std::vector<float>(static_cast<std::size_t>(RouteDraftInputs), 1.0f);
    j["out_mean"] = std::vector<float>(static_cast<std::size_t>(RouteDraftOutputs), 0.0f);
    j["out_std"] = std::vector<float>(static_cast<std::size_t>(RouteDraftOutputs), 1.0f);
    return j;
}

bool loads(const nlohmann::json& j, std::string* reason = nullptr)
{
    const std::string path = "route_draft_net_contract_test.json";
    {
        std::ofstream out(path);
        out << j.dump();
    }
    game::navigation::planner::RouteDraftNet net;
    const bool ok = net.load(path);
    if (reason)
        *reason = net.rejectReason();
    return ok;
}
} // namespace

int main()
{
    using game::navigation::planner::RouteDraftInputs;
    std::string why;
    check(loads(validModel(), &why), "a valid model loads (" + why + ")");

    const std::vector<std::pair<std::string, std::function<void(nlohmann::json&)>>> corruptions = {
        {"another schema", [](nlohmann::json& j) { j["schema"] = "elite.route_draft_net.v2"; }},
        {"another feature version", [](nlohmann::json& j) { j["feature_version"] = 99; }},
        {"inputs differ", [](nlohmann::json& j) { j["inputs"] = 111; }},
        {"ragged rows", [](nlohmann::json& j) { j["layers"][0]["w"][3].push_back(0.5f); }},
        {"long bias", [](nlohmann::json& j) { j["layers"][0]["b"].push_back(0.0f); }},
        {"short bias", [](nlohmann::json& j) { j["layers"][1]["b"].erase(0); }},
        {"a layer that does not chain", [](nlohmann::json& j) {
             for (auto& row : j["layers"][1]["w"])
                 row.push_back(0.0f);
         }},
        {"output width not 11", [](nlohmann::json& j) {
             j["layers"][1]["w"].erase(0);
             j["layers"][1]["b"].erase(0);
         }},
        {"short in_std", [](nlohmann::json& j) { j["in_std"].erase(0); }},
        {"zero std", [](nlohmann::json& j) { j["out_std"][2] = 0.0f; }},
        {"no layers", [](nlohmann::json& j) { j["layers"] = nlohmann::json::array(); }},
    };
    for (const auto& [name, corrupt] : corruptions)
    {
        nlohmann::json j = validModel();
        corrupt(j);
        std::string reason;
        const bool ok = loads(j, &reason);
        check(!ok && !reason.empty(), "refused: " + name + " (" + reason + ")");
        std::printf("  %-28s -> %s\n", name.c_str(), ok ? "LOADED" : reason.c_str());
    }
    // the shipped model
    game::navigation::planner::RouteDraftNet shipped;
    const bool shippedOk = shipped.load("src/assets/data/navigation/route_draft_net.json");
    check(shippedOk, "the shipped model meets the contract (" + shipped.rejectReason() + ")");
    if (shippedOk)
        check(shipped.forward(std::vector<float>(static_cast<std::size_t>(RouteDraftInputs), 0.0f)).size() == 11,
              "the shipped model gives 11 outputs");
    check(shipped.forward(std::vector<float>(5, 0.0f)).empty(), "a wrong-size feature vector gives no output");
    std::printf("route_draft_net_contract: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
