# Progress

2026-10-08: Reopened CI60 source fae491c and the original failing x86 log.
Source and local Microsoft/SDK contracts establish an expectation mismatch in
the shared-plus-primary descriptor path. ROOT agreed to a test-only exact
per-descriptor expectation and failure diagnostic, preserving production.
Strict optimized official-SDK x64/x86 compilation and source/log pin rejoin
are next. No local native GPU execution or case-index observation is claimed.

Original strict fixture compile processes 3237503/0 (x64) and 3237504/0 (x86) passed with empty diagnostics. Review 3238058/0 rejoined two actual COFF reopens and 284 selected dependencies. Source/log rejoin preserves every fixture byte after restoring the one corrected check, pins the exact 972150-byte CI log and the local Microsoft/SDK contracts. Actual native case index remains absent.
