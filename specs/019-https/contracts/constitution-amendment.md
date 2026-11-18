# Contract: Constitution amendment 1.3.0 → 1.4.0 (MINOR)

Added to "Implementation Constraints" (applied by the first task, with the Sync Impact Report updated):

> - Third-party source code (vendored code) MAY be built into the toolchain only under `third_party/`,
>   pinned to a released version whose archive checksum, license and list of kept files are recorded
>   in `third_party/<name>/VENDOR.md`. Vendored files are kept unmodified; a necessary change is a
>   patch file in `third_party/<name>/patches/` with its reason. Licenses must be permissive
>   (Apache-2.0, MIT, BSD).
> - Vendored code is built under the same constraints as the rest of the binary (freestanding, no
>   system libraries, `-ffp-contract=off`, no assembly, identical results on every supported platform)
>   and MUST pass the instrumented build. It is exempt from the project's strict warning set and from
>   the rules on authorship of C code: it compiles with its upstream's warnings, without
>   warnings-as-errors, and a new warning on update is reviewed.
> - A security release of a vendored library is applied in the spec cycle in which it appears.

Rationale line: HTTPS needs widely reviewed cryptography; writing it anew is riskier than vendoring an
LTS library, and the single-static-binary property is preserved.
