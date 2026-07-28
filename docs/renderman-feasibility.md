# RenderMan public-API feasibility gate

The repository does not claim a RenderMan integration from header presence or
from a procedural that expands dense triangles. The local gate is a
reproducible inspection of the pinned installation's public `Riley.h` surface:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run renderman-probe
jq . results/renderman-feasibility.json
```

On the current host this finds RenderManProServer 26.2 and the Riley scene
prototype/instance APIs (`CreateGeometryPrototype`, `CreateGeometryInstance`,
and `ModifyGeometryInstance`). It does not find public-header terms establishing
direct acceleration-structure ownership or custom intersection control. The
installed `ptrender` executable is evidence that a runtime is present, not that
the project has a licensed, runnable procedural prototype.

The result is therefore `status: partial` with a `no_go` decision for preserving
the tet-instancing fast path through public APIs. Closing M13 requires a pinned,
licensed runtime test that creates a bounded procedural or geometry prototype,
binds source primitive/material attributes, updates per-tet transforms, and
measures whether geometry remains immutable. If that test expands clipped
triangles per frame, it must be recorded as a negative result rather than called
an integration.
