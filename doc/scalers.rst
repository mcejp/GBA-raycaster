Rendering: Scalers
==================

32px vertical span x 2 columns (``kernelScale2Col32``)
------------------------------------------------------

- for 0.0 < scale <= 1.0 (1 to 32 pixels): decompose into 2x scaler 16px -> 1..16px (2492 bytes)
- for 1.0 < scale <= 2.0 (33 to 64 pixels): 2x scaler 16px -> 17..32px (6464 bytes)
- for 2.0 < scale <= 4.0 (65 to 128 pixels): 4x scaler 8px -> 16..32px (4920 bytes)
- for 4.0 < scale <= 8.0 (129 to 256 pixels): 8x scaler 4px -> 16..32px (4104 bytes)

When can we use sprites?
------------------------

- Whenever a >=8x8 part of a sprite is un-obscured by walls
    - how to detect this?
- Sprite data must fit into OAM (can we DMA just-in-time?)

How to order them?
^^^^^^^^^^^^^^^^^^
