# Findings

97bb captured original/slots/names and config before acquiring the adapter
lease. A second controller could capture the first controller's temporary
candidate, later fail prewrite equality after acquiring the lease, then replay
that unused stale candidate as an original in unconditional finally cleanup.

The correction separates readonly observation from authoritative Apply backup.
Apply acquires the adapter lease and refuses existing rescue tasks first, and
holds that lease across preparation/cleanup even when preparation throws.
Under the restore mutex, exact raw recheck precedes a protected complete flushed
mutation-intent record, which precedes RegSet. All restore actors consult that
intent. Absent intent means no raw replay; cancellation and fresh readback
equality are separate from actual restored bytes. A partial/failed RegSet after
intent still receives conservative full raw restoration.

This is source correction, not a Windows mutex/task/registry/GPU execution.
ROOT's actual Windows preflight and ownership-loss controls remain required.
