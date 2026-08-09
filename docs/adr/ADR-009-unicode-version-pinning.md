# ADR-009: Unicode version pinning

Status: accepted contract, implementation scheduled M3  
Date: 2026-08-07

Unicode behavior is pack-versioned and manifest-pinned. Updating Unicode data must never silently change canonical results produced under an existing resolved manifest. The scalar reference implementation, generated-table runtime, and later SIMD path all consume the same declared Unicode semantics version and are differential-tested.
