Debug menu
==========

It turns out the game is not small enough to avoid a debug menu.
It serves to:

- display player pose (coords & direction)
- jump to anchor

How it is implemented:

- main.cpp tracks if menu is active
- if so, input handling & drawing is redirected to ``dbgmenuInput`` and
  ``dbgmenuDraw``, respectively