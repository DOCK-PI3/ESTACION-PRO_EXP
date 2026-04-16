# Quick Start — Runtime Testing RetroAchievements

**Estimated Time**: 30-40 minutes  
**Difficulty**: Low  
**Requirements**: Display server (X11/Wayland), Network, RA Account

---

## Pre-Test Checklist (DO THIS FIRST)

```bash
# 1. Verify binary exists & is executable
ls -lh /home/mabe/Desktop/ESTACION-PRO/ESTACION-PRO

# 2. Check network connectivity
ping retroachievements.org

# 3. Have RA credentials ready
# You'll need:
# • RetroAchievements username
# • Password OR API token
# • (Save in a safe temporary location)
```

---

## Quick Test Execution

### TEST 1: Startup Auto-Indexing (5 min)

```bash
# 1. Enable auto-indexing in settings
# Option A: Edit ~/.config/ES-DE/es_settings.xml
# Find: <string name="CheevosCheckIndexesAtStart" value="false" />
# Change to: <string name="CheevosCheckIndexesAtStart" value="true" />
#
# Option B: Start app → Main Menu → System Settings → 
#            Frontend Developer Options → [Find RA Setting]

# 2. Create a test game directory (optional but recommended)
mkdir -p /tmp/test_roms/snes
# Copy a ROM file here (e.g., ~/Downloads/game.zip)

# 3. Point ES-DE to test directory in settings if needed

# 4. Launch the application
/home/mabe/Desktop/ESTACION-PRO/ESTACION-PRO &

# 5. WATCH FOR:
#    ✓ Splash screen appears
#    ✓ Progress popup shows: "Indexing achievements: X/Y games"
#    ✓ Counter increments smoothly
#    ✓ Completes with summary message
#    ✓ No crashes or freezes
```

**Success**: Popup appears, games get indexed, no crashes

---

### TEST 2: Manual Indexing (5-10 min)

```bash
# 1. App should still be running from TEST 1
# 2. Kill the indexer if still running (let it finish first)

# 3. Open Main Menu
#    Press START or equivalent button

# 4. Look for "RETROACHIEVEMENTS" entry
#    Click/Select it

# 5. Enter "RETROACHIEVEMENTS SETTINGS"

# 6. Look for button "RUN ACHIEVEMENT INDEX NOW"
#    Click it

# WATCH FOR:
#    ✓ Progress popup immediately appears
#    ✓ Real-time counter: "Indexing achievements: X/Y games"
#    ✓ Can press STOP button to halt
#    ✓ After stop, can click RUN again to resume

# Optional: Test stopping/restarting
#   - Click STOP after ~10 games
#   - Verify popup closes gracefully
#   - Click RUN again
#   - Verify it continues where it left off
```

**Success**: Menu buttons work, progress appears, stop/start responsive

---

### TEST 3: Metadata Persistence (2 min)

```bash
# After indexing completes:

# 1. Quit ESTACION-PRO

# 2. Open gamelist XML
gedit ~/.config/ES-DE/gamelists/snes/gamelist.xml
# OR wherever your test system's gamelist is

# 3. SEARCH FOR:
#    <cheevos_hash>
#    <cheevos_id>

# LOOK FOR:
#    ✓ cheevos_hash has 32-character hex value (MD5)
#    ✓ cheevos_id has numeric value or 0
#    ✓ Values persist across restart

# Example of what you should see:
#   <cheevos_hash>abc123def456...</cheevos_hash>
#   <cheevos_id>12345</cheevos_id>
```

**Success**: Metadata entries exist and contain valid values

---

### TEST 4: Theme Runtime Bindings (3-5 min)

```bash
# 1. Restart ESTACION-PRO

# 2. Navigate to the system with indexed games

# 3. LOOK FOR achievement indicators/badges
#    (Depends on your theme; may not show if theme doesn't bind to them)

# 4. If you see achievement icons/counters:
#    ✓ Games WITH cheevos_id > 0 show indicators
#    ✓ Games WITHOUT achievements don't show indicators
#    ✓ Scroll response remains smooth
#    ✓ No UI freezes when checking metadata

# To verify bindings in theme XML:
# Find your theme folder:
find ~/.config/ES-DE/themes -name "*.xml" | head -1
# Edit the gamelist.xml view
# Look for references like: ${game.cheevosId}
```

**Success**: Theme renders correctly with RA metadata available

---

### TEST 5: Menu Navigation (2-3 min)

```bash
# 1. App is running, showing gamelist

# 2. Press START to open Main Menu

# VERIFY:
#    ✓ "RETROACHIEVEMENTS" entry is visible
#    ✓ Entry shows description text
#    ✓ Can navigate to it with arrow keys
#    ✓ Can press OK to enter settings

# 3. Navigate to Settings → RETROACHIEVEMENTS SETTINGS

# VERIFY:
#    ✓ Menu loads without crashes
#    ✓ Can see credentials fields (username, etc.)
#    ✓ Can see index button
#    ✓ Can navigate back (B button or menu escape)
```

**Success**: Menu system responsive and discoverable

---

### TEST 6: Error Handling (Optional, 3-5 min)

```bash
# 1. Start indexing (TEST 2)

# 2. WHILE INDEXING, disconnect network
#    (Unplug ethernet OR disable WiFi)

# WATCH FOR:
#    ✓ App doesn't crash immediately
#    ✓ Graceful timeout (typically 5-10 seconds)
#    ✓ Either skips game or shows error
#    ✓ Can stop indexer without hanging

# 3. Reconnect network

# 4. Try indexing again

# WATCH FOR:
#    ✓ Resumes successfully with network back
#    ✓ No duplicate entries or corruption
```

**Success**: Network disconnection handled gracefully

---

## Logging & Debugging (If Issues Occur)

```bash
# 1. Check console output while running
#    (Any error messages in terminal?)
tail -f ~/.local/share/ES-DE/es_log.txt

# 2. Check for crash logs
#    (If app closes unexpectedly)
cd ~/.local/share/ES-DE/
ls -lt *.log *.txt 2>/dev/null | head

# 3. Manual MD5 calculation to verify hash logic
md5sum /path/to/test/game.zip

# 4. Check if cheevos metadata was saved
grep -i "cheevos" ~/.config/ES-DE/gamelists/*/gamelist.xml
```

---

## Results Template

```
Date: ______ 
Tester: ______ 
System: ______ (CPU/RAM)

Test Results:
  [ ] TEST 1: Auto-Indexing .............. PASS / FAIL
  [ ] TEST 2: Manual Menu Control ....... PASS / FAIL
  [ ] TEST 3: Metadata Persistence ...... PASS / FAIL
  [ ] TEST 4: Theme Runtime Bindings .... PASS / FAIL
  [ ] TEST 5: Menu Navigation ........... PASS / FAIL
  [ ] TEST 6: Error Handling ............ PASS / FAIL (Optional)

Performance Notes:
  - Startup time: _____ seconds
  - Index speed: _____ games/minute
  - Memory peak: _____ MB
  - UI responsiveness: Excellent / Good / Fair / Poor

Issues Found:
  1. _______________
  2. _______________

Overall Assessment: Ready for Integration / Needs Fixes / Blocked
```

---

## TroubleShooting Quick Guide

| Issue | Solution |
|-------|----------|
| No popup appears | Check CheevosCheckIndexesAtStart=true, verify network |
| Indexing hangs | Press ESC or check network connection |
| No metadata saved | Verify gamelist.xml is writable, check log.txt |
| Theme shows no badges | Theme may not implement RA bindings (expected) |
| Menu entry missing | Check RetroachievementsMenuitem=true in settings |
| Credentials rejected | Verify username/password correct, check token expiry |

---

## Success = All Tests PASS ✅

If all 6 tests pass, you can proceed to:
1. User documentation updates
2. Theme developer announcements  
3. General release/deployment

---

**Documentation Version**: 1.0  
**Created**: 2026-04-11  
**Valid For**: ESTACION-PRO builddate 2026-04-11 and later
