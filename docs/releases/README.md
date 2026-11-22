# Release records

Each tagged version receives an immutable record here: objective, shipped
capabilities, compatibility, artifact names and digests, and known
limitations. Release records are history and are not rewritten after
publication.

| Version | Date | Record |
| --- | --- | --- |
| — | — | No releases yet. v0.1.0 is planned at the end of Phase 1. |

## Release gate

A release record is created only after:

1. the version, tag, and finalized changelog heading agree;
2. every declared hosted CI cell passes;
3. source and package-origin gates pass at their documented levels, and the
   plugins are **discovered and open a fixture from the installed package**,
   not only from the source tree;
4. package digests are reproducible for an unchanged build;
5. third-party notices, SBOM and provenance policy, and target metadata are
   verified;
6. the release is assembled as a draft for human review.

The automation that enforces each item is documented here once the release
workflow exists.
