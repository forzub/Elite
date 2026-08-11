#include <type_traits>

#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/server/GameServer.h"
#include "src/game/simulation/GameSimulation.h"

int main()
{
    using game::ship::geometry::AssemblyMeshLibrary;
    using game::ship::geometry::ObjectAssembly;

    static_assert(
        std::is_same_v<
            decltype(AssemblyMeshLibrary::get(ObjectType::None)),
            const ObjectAssembly&
        >,
        "headless/server code must see only the shared CPU assembly definition"
    );

    // This target intentionally has no glad/OpenGL include directory or link
    // dependency. A future transitive GPU dependency from GameServer,
    // GameSimulation or AssemblyMeshLibrary therefore fails at compile time.
    static_assert(sizeof(GameServer) > 0);
    static_assert(sizeof(GameSimulation) > 0);

    return 0;
}
