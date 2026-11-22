# Architecture decision records

An ADR records one decision: its context, the options considered, the
decision, and its consequences. Accepted ADRs are normative. A later change of
mind is a new ADR that supersedes the old one; accepted ADRs are not rewritten
beyond status links and errata.

## Status values

| Status | Meaning |
| --- | --- |
| **Proposed** | The question and options are recorded; no decision is binding yet. |
| **Accepted** | Ratified by the maintainer; code and references must agree with it. |
| **Superseded** | Replaced by the named ADR. |
| **Rejected** | Considered and declined; kept for the record. |

## Index

| ADR | Title | Status | Needed by |
| --- | --- | --- | --- |
| [0001](0001-roadnetwork-data-model.md) | `RoadNetworkData` ID, topology, and geometry model | Proposed | Phase 1 decoder |
| [0002](0002-usd-schema-strategy.md) | USD schema strategy | Proposed | Phase 1 writer |
| [0003](0003-crs-axes-units.md) | CRS, axes, units, and large coordinates | Proposed | Phase 1 writer |
| [0004](0004-opendrive-geometry-retention.md) | OpenDRIVE curve and cross-section retention; tessellation | Proposed | Phase 1 decoder |
| [0005](0005-prim-path-and-id-stability.md) | Prim-path and source-ID stability; multiple networks | Proposed | Phase 1 writer |
| [0006](0006-fileformat-contract.md) | `SdfFileFormat` metadata-only, reload, and error contract | Proposed | Phase 0 scaffold (diagnostic numbering) |
| [0007](0007-geometry-lod-payload-boundary.md) | Geometry generation, LOD and tiles, payload boundary | Proposed | Phase 3 |
| [0008](0008-lanelet2-osm-identification.md) | Lanelet2 versus OSM identification; shared regulatory model | Proposed | Phase 4 |

## Writing an ADR

Use the next free number and a short kebab-case title. Include:

- **Status**, date, and scope;
- **Context** — the problem and the constraints from the
  [design policy](../design/DESIGN_POLICY.md);
- **Options considered**, with the reason each was taken or declined;
- **Decision**, written as rules code can be checked against;
- **Consequences** — the affected references, tests, and migration impact.

A Proposed ADR may also list the evidence still needed before acceptance.
Update the index row in the same PR.
