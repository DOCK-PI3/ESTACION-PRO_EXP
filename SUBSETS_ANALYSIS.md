# Análisis de Manejo de Subsets en ESTACION-PRO

## Resumen Ejecutivo
En ESTACION-PRO, los "subsets" no se llaman explícitamente así en el código. Los conceptos equivalentes son:
- **Variantes** (Variant - para selección de temas)
- **Esquemas de color** (ColorScheme)
- **Tamaños de fuente** (FontSize)
- **Idiomas** (Languages)
- **Storyboards** (para animaciones - el caso más similar a tu pregunta)

## 1. ¿Hay un default implícito (primer elemento)?

**SÍ, hay un default implícito: el primer elemento de la lista.**

### Patrón encontrado en `ThemeData.cpp` (líneas ~850-880):

```cpp
// Para Variantes
if (sCurrentTheme->second.capabilities.variants.size() > 0) {
    for (auto& variant : sCurrentTheme->second.capabilities.variants)
        mVariants.emplace_back(variant.name);

    if (std::find(mVariants.cbegin(), mVariants.cend(),
                  Settings::getInstance()->getString("ThemeVariant")) != mVariants.cend())
        mSelectedVariant = Settings::getInstance()->getString("ThemeVariant");
    else
        mSelectedVariant = mVariants.front();  // ← PRIMER ELEMENTO POR DEFECTO
}

// Para Color Schemes
if (sCurrentTheme->second.capabilities.colorSchemes.size() > 0) {
    for (auto& colorScheme : sCurrentTheme->second.capabilities.colorSchemes)
        mColorSchemes.emplace_back(colorScheme.name);

    if (std::find(mColorSchemes.cbegin(), mColorSchemes.cend(),
                  Settings::getInstance()->getString("ThemeColorScheme")) !=
        mColorSchemes.cend())
        mSelectedColorScheme = Settings::getInstance()->getString("ThemeColorScheme");
    else
        mSelectedColorScheme = mColorSchemes.front();  // ← PRIMER ELEMENTO POR DEFECTO
}

// Para Font Sizes
if (sCurrentTheme->second.capabilities.fontSizes.size() > 0) {
    for (auto& fontSize : sCurrentTheme->second.capabilities.fontSizes)
        mFontSizes.emplace_back(fontSize);

    if (std::find(mFontSizes.cbegin(), mFontSizes.cend(),
                  Settings::getInstance()->getString("ThemeFontSize")) != mFontSizes.cend())
        mSelectedFontSize = Settings::getInstance()->getString("ThemeFontSize");
    else
        mSelectedFontSize = mFontSizes.front();  // ← PRIMER ELEMENTO POR DEFECTO
}
```

**Lógica:**
1. Si existe una entrada en Settings (guardada anteriormente por el usuario), usa esa.
2. Si NO existe, usa `front()` → **el primer elemento de la colección**.

---

## 2. ¿Cómo se almacena la selección del usuario?

**Se almacena en `Settings` (persistente)** a través de strings específicos:

| Subset | Clave de Settings |
|--------|------------------|
| Variantes | `"ThemeVariant"` |
| Esquemas de color | `"ThemeColorScheme"` |
| Tamaños de fuente | `"ThemeFontSize"` |
| Aspect Ratio | `"ThemeAspectRatio"` |
| Idioma del tema | `"ThemeLanguage"` |

**Ejemplo de código:**
```cpp
// Verificar si hay una selección guardada
if (std::find(mVariants.cbegin(), mVariants.cend(),
              Settings::getInstance()->getString("ThemeVariant")) != mVariants.cend())
    mSelectedVariant = Settings::getInstance()->getString("ThemeVariant");
```

La selección se **busca en el vector de opciones disponibles**. Si la opción guardada ya no existe (ej: el tema fue actualizado y eliminó esa variante), se usa el default.

---

## 3. ¿Existe un valor por defecto para "storyboards" subset?

**SÍ, existe un mecanismo especial de priorización para storyboards.**

Esto está en `GuiComponent.cpp` líneas 519-531, en la función `selectStoryboard()`:

```cpp
bool GuiComponent::selectStoryboard(const std::string& name)
{
    auto sb = mStoryBoards.find(name);
    if (sb == mStoryBoards.cend()) {
        if (name.empty() && !mStoryBoards.empty()) {
            // Prioridad de selección por defecto para compatibilidad con themes
            // que usan eventos en storyboards.
            
            // 1. Primero: storyboard con nombre vacío ""
            if (mStoryBoards.find("") != mStoryBoards.cend())
                sb = mStoryBoards.find("");
            
            // 2. Luego: storyboard llamado "activate"
            else if (mStoryBoards.find("activate") != mStoryBoards.cend())
                sb = mStoryBoards.find("activate");
            
            // 3. Depois: storyboard llamado "show"
            else if (mStoryBoards.find("show") != mStoryBoards.cend())
                sb = mStoryBoards.find("show");
            
            // 4. Finalmente: el primer elemento de la lista
            else
                sb = mStoryBoards.begin();
        }
        else {
            return false;
        }
    }
    // ... resto del código
}
```

### Orden de Prioridad para Storyboards:

1. **Nombre vacío** `""` - Tiene la máxima prioridad
2. **"activate"** - Evento de activación
3. **"show"** - Evento de visualización
4. **Primer elemento** `mStoryBoards.begin()` - Default fallback

Este mecanismo es específico de storyboards y proporciona compatibilidad con temas que usan eventos en storyboards.

---

## Resumen Comparativo

| Aspecto | Variantes/ColorSchemes/FontSizes | Storyboards |
|--------|----------------------------------|------------|
| **Default Implícito** | Primer elemento (`.front()`) | Basado en nombres con prioridad |
| **Almacenamiento** | Settings persistente | Mapa en memoria (mapstring → shared_ptr) |
| **Selección** | Por nombre exacto | Por nombre exacto con fallback inteligente |
| **Especial** | Si no existe la guardada, usa default | Prioridad: "" > "activate" > "show" > primer elemento |

---

## Código Relevante en el Repositorio

- **ThemeData.cpp**: Líneas ~850-920 (carga y default de subsets)
- **ThemeData.cpp**: Líneas ~2238-2243 (asociación de storyboards a elementos)
- **GuiComponent.cpp**: Líneas ~427-431 (copia de storyboards desde tema)
- **GuiComponent.cpp**: Líneas ~519-531 (lógica de selección con prioridad)
- **Settings.cpp**: Manejo de persistencia (no inspeccionado en esta búsqueda)

