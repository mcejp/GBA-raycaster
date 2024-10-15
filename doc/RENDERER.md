## Sprites

### Additional assumptions related to sprites

- no sprite is taller than a wall (so sprites behind a wall are always fully occluded)
- no sprite is wider than a wall (so a sprite cannot peek from two sides of a corner 

  -> a sprite is never interrupted by a wall

### VRAM allocation

Pre-requisite: use OBJ for PSprite (so that layer ordering is correct)

- 16 KiB = 128x256 x 4-bit (or 32kpx)
- each sprite sheet is 128x128 (16kpx)
- psprite is 128x64 x 3 frames (24.6kpx but with symmetry, a lot of empty space + redundancy between frames)
 
### OAM allocation

- 0+1 are PSprite
