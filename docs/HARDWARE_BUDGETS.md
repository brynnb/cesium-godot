# Hardware-informed streaming budgets

`Cesium3DTileset` can report the hardware facts available through the running
Godot version and, when explicitly enabled, derive a bounded in-memory tile
cache from them. Automatic sizing is disabled by default. Existing projects
therefore retain Cesium Native's default or their authored
`maximum_cached_bytes` value without a behavior change.

This integration targets Cesium Native v0.63.0. Godot 4.6.3 is the minimum
supported runtime, and the same binary is tested through Godot 4.7.2.

## Capability snapshot

`get_hardware_capabilities()` returns one stable snapshot for the tileset:

- logical CPU count;
- physical and currently available system memory;
- rendering method, driver, adapter name, vendor, and adapter type when Godot
  reports them;
- whether a `RenderingDevice` exists;
- current renderer device-memory allocation; and
- total device memory when the active rendering device reports it.

An unavailable numeric fact is `-1`, never a guessed capacity. Headless and
compatibility renderers commonly have no `RenderingDevice`. The Godot 4.6.3
integration calls the typed renderer and device-capacity APIs directly; a
zero or otherwise unusable capacity is normalized to `-1`.

The snapshot follows Godot's public
[`OS.get_memory_info()`](https://docs.godotengine.org/en/4.6/classes/class_os.html#class-os-method-get-memory-info),
[`RenderingServer`](https://docs.godotengine.org/en/4.6/classes/class_renderingserver.html),
and
[`RenderingDevice`](https://docs.godotengine.org/en/4.6/classes/class_renderingdevice.html)
APIs. `device_used_memory_bytes` is Godot's current renderer allocation, not a
system-wide measurement of all VRAM consumers, and is diagnostic rather than a
physical ownership claim.

## Recommendation profiles

`get_recommended_total_cache_bytes(profile)` computes an application-wide
recommendation by taking the smallest known RAM/VRAM allowance, then applying
the profile cap:

| Profile | Physical RAM | Available RAM | Total VRAM | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Conservative | 10% | 25% | 25% | 2 GiB |
| Balanced | 20% | 50% | 50% | 8 GiB |
| Aggressive | 30% | 70% | 70% | 16 GiB |

Unknown inputs are ignored. If every capacity is unavailable, the result is
Cesium Native's established 512 MiB default. A known low-memory result is not
rounded upward: preserving available memory is more important than reaching a
nominal cache floor.

These values bound Cesium Native's logical loaded tile and raster-data cache.
They do not claim that every byte maps one-to-one to physical RAM or VRAM, and
they are independent of the SQLite HTTP disk cache.

## Opt-in application

Set `hardware_budget_profile`, assign this tileset's
`automatic_cache_budget_share`, and then enable
`automatic_hardware_budgets_enabled`. The default share is `1.0`. An
application with terrain and object tilesets might assign `0.4` and `0.6`; the
sum of shares should normally remain at or below `1.0`.

```gdscript
terrain.hardware_budget_profile = Cesium3DTileset.HardwareBudgetBalanced
terrain.automatic_cache_budget_share = 0.4
terrain.automatic_hardware_budgets_enabled = true

objects.hardware_budget_profile = Cesium3DTileset.HardwareBudgetBalanced
objects.automatic_cache_budget_share = 0.6
objects.automatic_hardware_budgets_enabled = true
```

Precedence is deliberate:

- enabling automatic sizing applies the current profile and share;
- changing the profile or share while enabled recalculates immediately;
- `recalculate_automatic_hardware_budgets()` refreshes the capability snapshot
  and reapplies it when automatic sizing is enabled;
- explicitly setting `maximum_cached_bytes` disables automatic sizing and is
  labeled `explicit`; and
- disabling automatic sizing keeps the last calculated byte value, labeled
  `automatic_frozen`, rather than silently reverting to another default.

`cache_budget_source`, the profile, share, enable state, and full capability
snapshot are included in `get_streaming_statistics()`.

## CPU worker default

The existing worker default remains logical CPU count minus one, clamped to
one through eight workers. Setting `worker_thread_count` makes it explicit.
`reset_worker_thread_count_to_automatic()` returns to the CPU-derived value,
and `worker_thread_count_source` distinguishes `automatic_cpu` from
`explicit` in the statistics snapshot.

Worker count and memory capacity are separate controls: more workers can
increase decode throughput and transient work, but does not enlarge the Native
tile cache. Applications should still benchmark their real content and frame
budgets rather than treating any hardware profile as universal tuning.
