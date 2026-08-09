# ADR-015: Routing evidence priority

Status: accepted contract, implementation scheduled after M4 unless a wedge requires it earlier  
Date: 2026-08-07

Language/profile routing evidence is ordered deterministically: explicit caller declaration; explicit document metadata; caller preferred set; script evidence; configured detector evidence; generic Unicode behavior; raw-byte fallback. Uncertainty reduces specialization rather than representability. Automatic routing never changes canonical source bytes or leaves.
