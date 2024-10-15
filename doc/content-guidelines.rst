Content guidelines
==================

Map authoring
-------------

- To (re-)generate tileset from palette, use a command such as this::

    python3 tools/jasc-generate-swatch.py assets/forest.pal -o forest.png --scale=16

- Sprite tileset must be updated manually if necessary

Image editing
-------------
- Aseprite is recommeded.

Sprites
^^^^^^^

A sprite is made of 4 animations:

- idle
- walking
- attacking
- dying

Palette
-------

- Custom palette created by merging from the sprites we want to use
- Very first color is magenta, representing transparency
- The next 16 colors correspond to Dawnbringer-16 (convenient for UI etc.)
- In sprites, color index 0 is forced to transparent!
  (we might change this to 255 but not sure if it's slower to check in the blitting loop)

Some alternative approaches:

- Design a custom one
- Look for a pre-made 256-color palette
- Data point: Wolf 3D uses a custom 256-color palette

Consider these palette modifiers:

- ~~damage taken~~ (GBA has color blending in HW)
- fog

Creating a palette
^^^^^^^^^^^^^^^^^^

1. create a big RGBA image in GIMP
2. paste everything that should be represented in palette
   (if necessary, decimate image palette before inserting)
3. convert to Indexed in GIMP using at most 239 colors, save PNG
4. load up in Aseprite & export palette
5. ensure that the palette starts with a transparent color followed by DB-16

Expected palette variations:

- damage (1x ?)
- fog (??x)

Content system assumptions
--------------------------

Game is small enough that:

- we can compile & re-link the entire ROM when any asset is edited
- assets can be linked into executable instead of using GBFS
  (GBA is ROM-based so there is no upfront cost for loading)
- we can manage any asset inter-dependencies manually
- we can do a full rebuild to solve dependency/staleness issues
  (e.g., palette change)
- we can manually manage sprite type IDs

Moreover,

- maps are small enough to compile into executable as C++ arrays & structures
- assets are few enough that the overhead of starting up Python for each transformation step
  is not a problem


General philosophy
------------------

- Use off-the-shelf editors!

  - Developing an editor is a bunch of work; developing a *good* editor is a career-long undertaking.

- It should be easy to rebuild all content (part of CMakeLists)


Naming convention
-----------------
- ``bm_`` benchmarking pose anchor
- ``img_`` single image or animation (e.g, PSprite)
- ``map_`` map
- ``pal_`` palette
- ``spr_`` sprite (collection of images/animations)
- ``tea_`` texture atlas
