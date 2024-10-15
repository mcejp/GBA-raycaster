Optimization opportunities
==========================

- Background fill is needlessly inefficient. Although we cannot use fast fill (DMA/BIOS) to fill columns, background ofter spans horizontally too.
  (The same applies to any solid-filled geometry)

- Ray casting is not the fastest algorithm (although we rather seem to be bottlenecked by the fill rate and sprite/texture scaling).
  BSP might work better.

- Shouldn't we copy sprites/textures from ROM to EWRAM?

- `SpriteSpan` doesn't need pointer to pixel data, just offset within array of full image

- If maps were of fixed size (2^n), tile addressing would be slightly faster

- Concurrency of VRAM access between display & rendering? Surely there is a specific optimal moment when to start drawing?

- If we assume that it is common for two consecutive screen columns to display two consecutive texture columns,
  maybe it would be worth packing texture columns in pairs and take advantage of it when this special case arises
  (we can use this data also in general case -- just advance the pointer by 2 bytes per row instead of 1)

In code: search for `// opt:`

General challenge: How to quantify effect of individual optimizations in isolation?
