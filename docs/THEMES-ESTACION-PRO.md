# Manual de temas de ESTACION-PRO

Este documento describe el motor de temas actual de ESTACION-PRO y explica todas las opciones compatibles en los archivos de tema.

## 1. ¿Qué es un tema en ESTACION-PRO?

Un tema en ESTACION-PRO es un conjunto de archivos XML y recursos gráficos que definen la apariencia de la aplicación:

- `theme.xml` o `themes/<nombre-tema>/<sistema>/theme.xml`
- Imágenes, iconos, vídeos y sonidos referenciados desde el XML
- Archivos adicionales incluidos mediante `<include>` y `<subset>`

ESTACION-PRO carga los temas desde los directorios de tema del sistema o del usuario. Los recursos se resuelven en base a la ubicación del archivo XML que los incluye.

## 2. Ubicación del tema y estructura básica

El motor de temas utiliza el siguiente patrón:

- `themes/<tema>/<sistema>/theme.xml`
- En algunos casos también puede haber un `themes/<tema>/theme.xml` o archivos incluidos desde él.

Un tema básico puede tener esta estructura:

```xml
<theme>
  <variables>
    <variable name="background" value="background.png"/>
  </variables>

  <include file="shared.xml"/>

  <subset name="systemInfo">
    <include name="normal" file="theme-normal.xml"/>
    <include name="compact" file="theme-compact.xml"/>
  </subset>

  <variant name="gamelist">
    <view name="gamelist">
      <image name="background" pos="0 0" size="1 1" path="${background}"/>
    </view>
  </variant>
</theme>
```

## 3. Vistas soportadas

ESTACION-PRO entiende las siguientes vistas:

- `all`
- `system`
- `gamelist`
- `menu`
- `screen`
- `splash`
- `basic`
- `detailed`
- `grid`

Además de estas vistas, existen:

- `<customView name="...">...</customView>` para estilos de gamelist alternativos
- `<variant name="...">...</variant>` para variantes seleccionables del tema
- `<colorScheme name="...">...</colorScheme>` para esquemas de color
- `<fontSize name="...">...</fontSize>` para tamaños de letra
- `<aspectRatio name="...">...</aspectRatio>` para relaciones de aspecto
- `<language name="...">...</language>` para temas localizados

## 4. Elementos principales y su uso

Cada elemento del tema se define con una etiqueta XML y un atributo `name` obligatorio.

Ejemplo de elemento:

```xml
<image name="background" pos="0 0" size="1 1" path="background.png"/>
```

Se acepta `name` con varios valores separados por comas para crear múltiples elementos con la misma definición.

### Alias y compatibilidad Batocera

ESTACION-PRO también admite compatibilidad con etiquetas Batocera/Retrobat:

- `<gamecarousel>` se mapea a `<carousel>`
- `<batteryText>` se mapea a `<text>`
- `<networkIcon>` se mapea a `<image>`
- `<webimage>` se mapea a `<image>`
- `<control>` no crea un componente nuevo, sino que modifica elementos existentes

## 5. Tipos de datos y formatos de propiedad

Las propiedades admiten varios tipos:

- `PATH`: ruta de recurso (imagen, vídeo, sonido, fuente)
- `STRING`: texto cualquiera
- `COLOR`: color en formato hexadecimal RGB o ARGB
- `FLOAT`: valor decimal
- `BOOLEAN`: `true/false`, `1/0`, `yes/no`, `on/off`
- `UNSIGNED_INTEGER`: número entero positivo
- `NORMALIZED_PAIR`: par normalizado, p.ej. `0.1 0.2`
- `NORMALIZED_RECT`: rectángulo normalizado, p.ej. `0.1 0.1 0.8 0.3`

Las coordenadas `pos` y `size` suelen expresarse en valores normalizados entre `0.0` y `1.0`, relativos a la vista.

## 6. Propiedades comunes para casi todos los elementos

Estas opciones están disponibles en muchos elementos:

- `pos` — posición normalizada `x y`
- `size` — tamaño normalizado `w h`
- `x`, `y`, `w`, `h` — valores individuales en coma flotante
- `origin` — punto de origen del elemento `x y`
- `rotation` — rotación en grados
- `rotationOrigin` — centro de rotación `x y`
- `scale` — escala global
- `scaleOrigin` — punto de origen de la escala `x y`
- `padding` — relleno `left top right bottom`
- `clipChildren` — recorta hijos fuera del área
- `opacity` — opacidad del elemento
- `visible` — visibilidad del elemento
- `zIndex` — orden de apilado
- `color` — color principal
- `colorEnd` — segundo color para degradados
- `gradientType` — tipo de degradado
- `backgroundColor` — color de fondo
- `backgroundColorEnd` — color final del fondo
- `fontPath` — ruta a fuente
- `fontSize` — tamaño de fuente
- `letterCase` — mayúsculas/minúsculas automáticas
- `lineSpacing` — espaciado entre líneas
- `text` — texto literal
- `path` — ruta de recurso
- `default` / `defaultImage` — recurso alternativo si no hay media disponible
- `metadataElement` — indica que el elemento usa datos del juego
- `gameselector` — selector de juego activo
- `gameselectorEntry` — entrada específica del selector

## 7. Elementos principales soportados

Aquí están los tipos de elementos más habituales y sus propiedades clave.

### 7.1 `image`

Usado para imágenes de fondo, logos, iconos, banners, marcos y más.

Propiedades importantes:

- `path`
- `gameOverridePath`
- `default`
- `imageType`
- `metadataElement`
- `gameselector`
- `gameselectorEntry`
- `tile`
- `tileSize`
- `tileHorizontalAlignment`
- `tileVerticalAlignment`
- `horizontalAlignment`
- `interpolation`
- `linearSmooth`
- `mipmap`
- `cornerRadius`
- `reflexion`
- `reflexionOnFrame`
- `brightness`
- `saturation`
- `opacity`

Ejemplo:

```xml
<image name="game_image" pos="0.05 0.1" size="0.4 0.6"
       path="media/images/${system.theme}/${game.name}.png"
       default="default_game.png"
       imageType="cover"
       opacity="0.95"/>
```

### 7.2 `video`

Reproduce vídeos de juego en gamelist o vistas de detalle.

Propiedades más usadas:

- `path`
- `default`
- `defaultImage`
- `imageType`
- `metadataElement`
- `gameselector`
- `gameselectorEntry`
- `iterationCount`
- `onIterationsDone`
- `audio`
- `interpolation`
- `roundCorners`
- `videoCornerRadius`
- `pillarboxes`
- `pillarboxThreshold`
- `scanlines`
- `delay`
- `fadeInType`
- `fadeInTime`
- `showSnapshotNoVideo`
- `showSnapshotDelay`
- `showVideoAtDelay`
- `effect`
- `snapshotSource`
- `reflexion`
- `reflexionOnFrame`

Ejemplo:

```xml
<video name="game_video" pos="0.5 0.1" size="0.45 0.5"
       path="media/videos/${game.name}.mp4"
       defaultImage="default_video.png"
       imageType="screenshot"
       audio="true"
       showVideoAtDelay="0.2"
       showSnapshotNoVideo="true"
       loop="true"/>
```

### 7.3 `text`

Muestra texto estático o metadatos del juego.

Propiedades importantes:

- `text`
- `systemdata`
- `metadata`
- `defaultValue`
- `fontPath`
- `fontSize`
- `horizontalAlignment`
- `verticalAlignment`
- `color`
- `backgroundColor`
- `backgroundMargins`
- `backgroundCornerRadius`
- `letterCase`
- `lineSpacing`
- `glowColor`
- `glowSize`
- `glowOffset`
- `forceUppercase`
- `opacity`

Ejemplo:

```xml
<text name="md_name" pos="0.05 0.05" size="0.9 0.08"
      metadata="name"
      fontPath="fonts/OpenSans-Regular.ttf"
      fontSize="0.045"
      color="0xFFFFFFFF"
      glowColor="0xFFFFFFFF"
      glowSize="0.01"/>
```

### 7.4 `textlist`

Define la lista de juegos en el gamelist view.

Propiedades clave:

- `selectorWidth`, `selectorHeight`
- `selectorHorizontalOffset`, `selectorVerticalOffset`
- `selectorOffsetY`
- `selectorColor`, `selectorColorEnd`, `selectorGradientType`
- `selectorImagePath`, `selectorImageTile`
- `primaryColor`, `secondaryColor`
- `selectedColor`, `selectedSecondaryColor`
- `selectedBackgroundColor`
- `selectedBackgroundMargins`
- `selectedBackgroundCornerRadius`
- `textHorizontalScrolling`
- `textHorizontalScrollSpeed`
- `textHorizontalScrollDelay`
- `textHorizontalScrollGap`
- `horizontalAlignment`
- `horizontalMargin`
- `letterCase`, `letterCaseAutoCollections`, `letterCaseCustomCollections`
- `lineSpacing`
- `indicators`, `collectionIndicators`
- `fadeAbovePrimary`
- `scrollSound`
- `forceUppercase`

Ejemplo:

```xml
<textlist name="gamelist" pos="0.05 0.2" size="0.4 0.75"
          selectorWidth="0.37"
          selectorColor="0xFFAAAAFF"
          fontPath="fonts/OpenSans-Regular.ttf"
          fontSize="0.032"
          textHorizontalScrolling="true"
          textHorizontalScrollSpeed="0.03"
          indicators="true"/>
```

### 7.5 `grid`

Lista de juegos en diseño de cuadrícula.

Propiedades clave:

- `itemSize`
- `itemSpacing`
- `scaleInwards`
- `fractionalRows`
- `rowTransitions`
- `itemTransitions`
- `unfocusedItemOpacity`
- `imageFit`
- `imageCropPos`
- `imageCornerRadius`
- `backgroundImage`
- `selectorImage`
- `selectorLayer`
- `logoSize`, `logoScale`, `logoAlignment`
- `animateSelection`
- `selectionMode`

Ejemplo:

```xml
<grid name="game_grid" pos="0.05 0.1" size="0.9 0.8"
      itemSize="0.18 0.25"
      itemSpacing="0.02 0.02"
      imageFit="cover"
      selectorImage="selector.png"
      animateSelection="true"/>
```

### 7.6 `carousel`

Carrusel de juegos o de sistemas.

Propiedades principales:

- `type`
- `staticImage`
- `imageSource`
- `defaultImage`
- `defaultFolderImage`
- `itemsBeforeCenter`, `itemsAfterCenter`
- `itemStacking`
- `selectedItemMargins`
- `itemSize`, `itemScale`
- `imageFit`
- `imageCornerRadius`
- `logoSize`, `logoScale`, `logoAlignment`
- `reflections`, `reflectionsOpacity`, `reflectionsFalloff`
- `wheelHorizontalAlignment`, `wheelVerticalAlignment`
- `fastScrolling`
- `color`, `colorEnd`, `gradientType`

Ejemplo:

```xml
<carousel name="system_carousel" pos="0.05 0.1" size="0.9 0.8"
          type="horizontal_wheel"
          itemsBeforeCenter="3"
          itemsAfterCenter="3"
          imageFit="cover"
          reflections="true"
          reflectionsOpacity="0.4"/>
```

### 7.7 `imagegrid`

Diseño de cuadrícula especializado para imágenes con desplazamiento.

Propiedades útiles:

- `autoLayout`
- `autoLayoutSelectedZoom`
- `scrollDirection`
- `centerSelection`
- `scrollLoop`
- `scrollSound`
- `selectionMode`
- `margin`
- `padding`

Ejemplo:

```xml
<imagegrid name="game_gallery" pos="0.05 0.1" size="0.9 0.8"
           itemSize="0.25 0.25"
           scrollDirection="horizontal"
           centerSelection="true"
           scrollLoop="true"
           autoLayout="0.05 0.05"/>
```

### 7.8 `rating`

Muestra un valor de puntuación visual.

Propiedades clave:

- `hideIfZero`
- `filledPath`
- `unfilledPath`
- `overlay`
- `interpolation`

Ejemplo:

```xml
<rating name="game_rating" pos="0.05 0.8" size="0.3 0.08"
        filledPath="star_filled.png"
        unfilledPath="star_empty.png"
        color="0xFFFFFF"/>
```

### 7.9 `datetime`

Formatea fechas y horas.

Propiedades importantes:

- `metadata`
- `format`
- `displayRelative`

Ejemplo:

```xml
<datetime name="md_lastplayed" pos="0.05 0.9" size="0.4 0.06"
          metadata="lastplayed"
          format="%d %b %Y"/>
```

### 7.10 `gamelistinfo`

Texto adicional específico para gamelist.

### 7.11 `helpsystem`

Define la ayuda de botones y accesos directos.

Propiedades clave:

- `scope`
- `entries`
- `entryLayout`
- `entryRelativeScale`
- `entrySpacing`
- `iconUpDown`, `iconA`, `iconB`, etc.

### 7.12 `systemstatus`

Muestra iconos de estado del sistema.

Propiedades importantes:

- `entries`
- `entrySpacing`
- `customIcon`

### 7.13 `clock`

Reloj en pantalla.

Propiedades importantes:

- `format`
- `displayRelative`
- `fontPath`
- `fontSize`

### 7.14 `sound`

Reproduce efectos de sonido.

- `path`

### 7.15 `badges`

Muestra insignias e iconos de control.

Propiedades importantes:

- `customBadgeIcon`
- `customControllerIcon`
- `badgeIconColor`
- `controllerIconColor`

### 7.16 `ninepatch`

Cajas de borde de 9 parches.

- `path`
- `cornerSize`
- `centerColor`
- `edgeColor`

### 7.17 Menús y elementos UI

Elementos para la interfaz de configuración y navegación:

- `menuBackground`
- `menuIcons`
- `menuText`
- `menuTextSmall`
- `menuTextEdit`
- `menuSwitch`
- `menuSlider`
- `menuButton`
- `menuGroup`
- `menuGrid`

## 8. Soporte de medios y tipos de imagen

Las rutas de media pueden usar el valor de `imageType` para seleccionar la fuente correcta.

Tipos admitidos para `imageType` y `metadataElement`:

- `miximage`
- `marquee`
- `screenshot`
- `titlescreen`
- `cover`
- `backcover`
- `3dbox`
- `physicalmedia`
- `fanart`
- `video`

## 9. Variables dinámicas y condiciones

ESTACION-PRO admite variables dinámicas dentro de texto y rutas.

### Sintaxis de variables

- `{global:help}`
- `{global:clock}`
- `{global:architecture}`
- `{global:language}`
- `{global:region}`
- `{global:cheevos}`
- `{global:cheevosuser}`
- `{global:netplay}`
- `{global:netplay.username}`
- `{global:battery}`
- `{global:batterylevel}`
- `{global:screen.width}`
- `{global:screen.height}`
- `{global:screen.ratio}`
- `{global:screen.vertical}`
- `{screen:width}`
- `{screen:height}`
- `{screen:ratio}`
- `{screen:vertical}`
- `{system:name}`
- `{system:theme}`
- `{collection:*}`
- `{game:*}`
- `{subset:*}`
- `{lang}`, `{language}`, `{locale}`
- `{themepath}`
- `{currentpath}`

### Ejemplo de uso

```xml
<image name="logo" pos="0.05 0.05" size="0.3 0.15"
       path="${themepath}/logos/{system:theme}.png"/>
```

### Condiciones `if`

Las etiquetas Batocera pueden usar atributos `if` para activar o desactivar elementos.

Ejemplo:

```xml
<text name="cheevos_status" pos="0.05 0.92" size="0.9 0.06"
      text="Logros activos"
      visible="true"
      if="${global.cheevos} == true"/>
```

También puede usar comparaciones con `${screen.width}`, `${screen.height}` y `${screen.ratio}`.

### Ejemplo real de condición

```xml
<image name="vertical_banner" pos="0.8 0.05" size="0.15 0.9"
       path="vertical.png"
       if="${screen.vertical} == true"/>
```

## 10. Variantes, esquemas y subsets

### Variantes (`<variant>`)

Permiten seleccionar diferentes estilos o gamelist views.

```xml
<variant name="gamelist grid"/>
<variant name="gamelist textlist"/>
```

### Esquemas de color (`<colorScheme>`)

Define conjuntos de colores adicionales que puede elegir el usuario.

```xml
<colorScheme name="dark"/>
<colorScheme name="bright"/>
```

### Tamaños de fuente (`<fontSize>`)

Para temas que admiten distintos tamaños de letra.

```xml
<fontSize name="large"/>
<fontSize name="small"/>
```

### Relaciones de aspecto (`<aspectRatio>`)

Permite cargar variantes de tema específicas para pantallas 16:9, 4:3, vertical, etc.

```xml
<aspectRatio name="16:9"/>
<aspectRatio name="4:3"/>
```

### Idiomas (`<language>`)

Permite definir bloques localizados.

```xml
<language name="es_ES"/>
<language name="en_US"/>
```

### Subsets (`<subset>`)

Permiten agrupar opciones que cargan archivos adicionales.

```xml
<subset name="systemInfo">
  <include name="normal" file="theme-normal.xml"/>
  <include name="mini" file="theme-mini.xml"/>
</subset>
```

Dentro del subset, el motor carga solo la opción seleccionada.

## 11. Controles y modificaciones de elementos existentes

El elemento `<control>` se usa para modificar elementos ya declarados.

Ejemplo:

```xml
<control name="md_name" pos="0.05 0.15" size="0.7 0.07"/>
```

En temas Batocera, `<control>` se emplea para ajustar posiciones, colores o visibilidad sin recrear la etiqueta original.

## 12. Ejemplos completos

### 12.1 Tema mínimo para `gamelist`

```xml
<theme>
  <view name="gamelist">
    <image name="background" pos="0 0" size="1 1" path="background.png"/>
    <text name="md_name" pos="0.05 0.05" size="0.5 0.08"
          metadata="name"
          fontPath="fonts/OpenSans-Regular.ttf"
          fontSize="0.05"
          color="0xFFFFFFFF"/>
    <video name="md_video" pos="0.55 0.1" size="0.4 0.4"
           defaultImage="default_video.png"
           audio="true"
           showVideoAtDelay="0.2"/>
    <textlist name="gamelist" pos="0.05 0.25" size="0.45 0.7"
              selectorColor="0xFF00FF00"
              textHorizontalScrolling="true"
              fontPath="fonts/OpenSans-Regular.ttf"
              fontSize="0.035"/>
  </view>
</theme>
```

### 12.2 Tema con `carousel` y `systemstatus`

```xml
<theme>
  <view name="system">
    <image name="background" pos="0 0" size="1 1" path="system_bg.png"/>
    <carousel name="system_carousel" pos="0.05 0.15" size="0.9 0.65"
              type="horizontal_wheel"
              itemsBeforeCenter="2"
              itemsAfterCenter="2"
              reflections="true"
              reflectionsOpacity="0.35"/>
    <systemstatus name="system_status" pos="0.05 0.85" size="0.9 0.1"
                  entries="wifi,battery,bluetooth"/>
  </view>
</theme>
```

## 13. Bucles y overlays comunes de tema

### `gamecarousel` en temas Batocera

`<gamecarousel>` se interpreta como `<carousel>` sin cambios adicionales.

### `batteryText`, `networkIcon`, `webimage`

Se mapean a elementos compatibles de ESTACION-PRO, por lo que un tema Batocera puede reutilizar estas etiquetas.

## 14. Consejos de depuración

- Verifica que cada elemento tenga `name`.
- Asegúrate de que las rutas `path` sean correctas y relativas a la carpeta del XML.
- Comprueba que los atributos `if` y las variables dinámicas estén bien formateados.
- Activa `DebugSkipMissingThemeFiles` si estás probando un tema incompleto.
- Revisa los nombres de vista admitidos: `system`, `gamelist`, `menu`, `screen`, `splash`, `basic`, `detailed`, `grid`.

## 15. Buenas prácticas para crear temas

- Usa `defaultImage` y `default` para evitar espacios negros si falta un recurso.
- Prioriza `imageType` y `metadataElement` para que la vista extraiga correctamente la información del juego.
- Para el gamelist, usa `gamelist` o `customView` con herencia para mantener compatibilidad.
- Separa configuraciones por `subset` cuando el tema tenga variantes de tamaño o estilo.
- Prueba en varias relaciones de aspecto usando `<aspectRatio>`.

## 16. Compatibilidad con Batocera/Retrobat

ESTACION-PRO es compatible con muchas etiquetas y propiedades de Batocera, pero también añade mejoras propias.

### Lo que funciona directamente

- Elementos de imagen y texto estándar
- `<gamecarousel>`
- Alias `batteryText`, `networkIcon`, `webimage`
- Uso de `metadataElement` y `gameselector`
- Propiedades de vídeo avanzadas como `showVideoAtDelay`, `scanlines`, `pillarboxes`

### Recomendaciones

- Si usas temas Batocera, verifica los nombres de etiqueta y ajusta `imageType` a los media types soportados.
- Usa variables dinámicas con la sintaxis `{global:...}`, `{system:...}` y `{game:...}`.
- Para temas modernos, utiliza `customView` y `variant` en lugar de definir bloques separados de gamelist.

---

Este manual es la referencia actualizada para crear y adaptar temas en ESTACION-PRO. Usa los ejemplos como punto de partida y ajusta las propiedades según el diseño que quieras lograr.