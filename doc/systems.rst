============
Game systems
============

Entities
========

Current implementation: small map, all entities in an array, all processed all the time

Improvement: only process entities within certain manhattan distance from player?

Enemy AI
--------

- enemy state: IDLE (player not seen), PURSUIT, ATTACKING

  - PURSUIT sub-states: ATTACKING, COOLDOWN ?

- each enemy does their own FOV check + trace to player (1 enemy each frame, or something.)


Movement & collision detection
==============================

Principle: player is a square. (not a circle ?)

When movement is requested, we compute the extents of the square in new position and check if it intersects a wall.
If it does, we reject the movement (without prejudice for future frames)

Sliding along walls would definitely be desirable, but is not implemented at the moment.