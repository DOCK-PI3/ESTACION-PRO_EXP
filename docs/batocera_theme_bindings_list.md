# Lista de propiedades y elementos de THEMES.md (Batocera)

## Vistas y elementos principales

- basic
- detailed
- video
- grid
- system
- menu
- screen

## Elementos comunes
- helpsystem
- image (background, logo, md_image, gridtile.marquee, etc.)
- text (logoText, md_lbl_*, md_*, gridtile, gridtile_selected, etc.)
- textlist (gamelist)
- imagegrid (gamegrid)
- gridtile (default, selected)
- video (md_video, extras)
- rating (md_rating)
- datetime (md_releasedate, md_lastplayed, etc.)
- ninepatch
- sound
- controllerActivity
- menuText, menuTextSmall, menuSwitch, menuButton, menuTextEdit, menuIcons
- carousel (systemcarousel)

## Propiedades comunes
- pos (NORMALIZED_PAIR)
- size (NORMALIZED_PAIR)
- origin (NORMALIZED_PAIR)
- path (PATH)
- color (COLOR)
- fontPath (PATH)
- fontSize (FLOAT)
- alignment (STRING)
- visible (BOOLEAN)
- zIndex (FLOAT)
- scale (FLOAT)
- scaleOrigin (NORMALIZED_PAIR)
- rotation (FLOAT)
- rotationOrigin (NORMALIZED_PAIR)
- padding (NORMALIZED_RECT)
- x, y, w, h (FLOAT)
- extra (BOOLEAN)

## Propiedades específicas de carousel (gamecarrousel)
- type (STRING: horizontal, vertical, horizontal_wheel, vertical_wheel)
- size (NORMALIZED_PAIR)
- pos (NORMALIZED_PAIR)
- origin (NORMALIZED_PAIR)
- color (COLOR)
- logoSize (NORMALIZED_PAIR)
- logoScale (FLOAT)
- logoRotation (FLOAT)
- logoRotationOrigin (NORMALIZED_PAIR)
- logoAlignment (STRING: top, bottom, center, left, right)
- maxLogoCount (FLOAT)
- colorEnd (COLOR)
- gradientType (STRING)
- logoPos (NORMALIZED_PAIR)
- systemInfoDelay (FLOAT)

## Propiedades específicas de imagegrid
- margin (NORMALIZED_PAIR)
- gameImage (PATH)
- folderImage (PATH)
- scrollDirection (STRING)
- autoLayout (NORMALIZED_PAIR)
- autoLayoutSelectedZoom (FLOAT)
- imageSource (STRING: thumbnail, image, marquee)
- showVideoAtDelay (FLOAT)
- scrollSound (STRING)
- centerSelection (BOOL)
- scrollLoop (BOOL)
- animateSelection (BOOL)

## Propiedades específicas de gridtile
- size (NORMALIZED_PAIR)
- padding (NORMALIZED_PAIR)
- imageColor (COLOR)
- backgroundImage (PATH)
- backgroundCornerSize (NORMALIZED_PAIR)
- backgroundColor (COLOR)
- backgroundCenterColor (COLOR)
- backgroundEdgeColor (COLOR)
- selectionMode (STRING)
- imageSizeMode (STRING)
- reflexion (NORMALIZED_PAIR)

## Propiedades específicas de text
- text (STRING)
- color (COLOR)
- backgroundColor (COLOR)
- fontPath (PATH)
- fontSize (FLOAT)
- alignment (STRING)
- forceUppercase (BOOLEAN)
- lineSpacing (FLOAT)
- glowColor (COLOR)
- glowSize (FLOAT)
- glowOffset (FLOAT)
- reflexion (NORMALIZED_PAIR)
- reflexionOnFrame (BOOLEAN)
- padding (NORMALIZED_RECT)

## Propiedades específicas de video
- path (PATH)
- default (PATH)
- showSnapshotNoVideo (BOOLEAN)
- showSnapshotDelay (BOOLEAN)
- snapshotSource (STRING)
- effect (STRING)

## Propiedades específicas de controllerActivity
- itemSpacing (FLOAT)
- horizontalAlignment (STRING)
- imagePath (PATH)
- color (COLOR)
- activityColor (COLOR)
- hotkeyColor (COLOR)

## Animaciones (storyboard)
- storyboard (elemento)
- animation (propiedad, from, to, begin, duration, repeat, autoReverse, mode)

---

Esta lista se ha generado a partir de THEMES.md de Batocera (https://github.com/batocera-linux/batocera-emulationstation/blob/master/THEMES.md) y servirá como referencia para futuras implementaciones y comparativas.
