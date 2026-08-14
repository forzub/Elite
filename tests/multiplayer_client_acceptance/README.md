# Multiplayer client acceptance

Run from the repository root under MSYS2 MinGW64:

```bash
bash tests/multiplayer_client_acceptance/run_mingw64.sh
```

This gate boots one production `ServerRuntime`, two independent
`LocalLoopbackTransport` endpoints and two real `GameClient` state machines.
The server assigns a different controlled entity to each session. The clients
must both reach `Ready`, retain the same replicated world, derive navigation
from their own controlled entity, treat the other human-controlled ship as
remote, and maintain independent numbered input/acknowledgement streams.

The harness deliberately does not instantiate or mutate `GameServer` or
`GameSimulation` directly and does not forge protocol messages. It exercises
the same `GameClient -> ITransport -> ServerRuntime/ServerRunner` path that a
future socket transport will use.
