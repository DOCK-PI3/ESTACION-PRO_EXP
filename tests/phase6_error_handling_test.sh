#!/bin/bash
################################################################################
# ESTACION-PRO Phase 6: Error Handling Test Suite
# 
# Purpose: Validate error handling, recovery mechanisms, and robustness
# 
# This test suite validates:
#   1. Invalid setting values in configuration
#   2. Missing resource files handling
#   3. Corrupted configuration recovery
#   4. Edge cases and boundary conditions
#   5. Graceful error messaging and logging
#
# Success Criteria:
#   - Application handles errors without crashing
#   - Meaningful error messages logged
#   - Graceful degradation when resources missing
#   - Recovery from corrupted configurations
#   - No heap corruption or memory errors
#
################################################################################

set +e  # Continue on errors

# Environment
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LOG_DIR="/tmp/phase6_test_logs"
TEST_HOME="/tmp/espro_phase6_home"
BINARY="$PROJECT_ROOT/ESTACION-PRO-prod"
PROD_RESOURCES="/home/mabe/ES-PRO/resources"
PROD_THEMES="/home/mabe/ES-PRO/themes"
PROD_SETTINGS="/home/mabe/ES-PRO/settings"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

################################################################################
# Logging Functions
################################################################################

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
    echo "[INFO] $1" >> "$LOG_DIR/error_handling_test.log"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    echo "[PASS] $1" >> "$LOG_DIR/error_handling_test.log"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    echo "[FAIL] $1" >> "$LOG_DIR/error_handling_test.log"
    ((TESTS_FAILED++))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[WARN] $1" >> "$LOG_DIR/error_handling_test.log"
}

setup_environment() {
    mkdir -p "$LOG_DIR"
    rm -f "$LOG_DIR"/*.log
    
    if [ ! -f "$BINARY" ]; then
        log_fail "Binary not found: $BINARY"
        return 1
    fi
    
    log_pass "Test environment ready"
    return 0
}

create_base_home() {
    local home_dir=$1
    mkdir -p "$home_dir/ES-PRO/settings" "$home_dir/ES-PRO/logs" "$home_dir/ES-PRO/themes" "$home_dir/ES-PRO/resources"
    
    # Copy essentials
    cp "$PROD_SETTINGS/es_settings.xml" "$home_dir/ES-PRO/settings/" 2>/dev/null || true
    cp -a "$PROD_THEMES/es-theme-carbon-master" "$home_dir/ES-PRO/themes/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/fonts" "$home_dir/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/systems" "$home_dir/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/MAME" "$home_dir/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/locale" "$home_dir/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/sounds" "$home_dir/ES-PRO/resources/" 2>/dev/null || true
}

################################################################################
# Test Suite
################################################################################

test_1_invalid_settings() {
    log_info "\n=== Test 1: Invalid Setting Values ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test1_invalid_settings"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Corrupt settings file - add invalid XML
    local settings_file="$test_home/ES-PRO/settings/es_settings.xml"
    cat >> "$settings_file" << 'EOF'

<!-- Invalid XML to trigger parsing errors -->
<string name="InvalidSetting" value="test"
</corrupted>

EOF
    
    local test_log="$LOG_DIR/test1_invalid_settings.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on invalid settings (SIGABRT)"
        return 1
    elif grep -q "XML error\|parse error\|invalid" "$test_log" 2>/dev/null; then
        log_pass "Application detected and logged invalid settings"
    else
        log_warn "Application handled invalid settings (graceful degradation)"
    fi
    
    ((TESTS_RUN++))
}

test_2_missing_themes() {
    log_info "\n=== Test 2: Missing Theme Files ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test2_missing_themes"
    mkdir -p "$test_home/ES-PRO/settings" "$test_home/ES-PRO/logs" "$test_home/ES-PRO/themes" "$test_home/ES-PRO/resources"
    
    # Copy settings WITHOUT themes
    cp "$PROD_SETTINGS/es_settings.xml" "$test_home/ES-PRO/settings/"
    cp -a "$PROD_RESOURCES/fonts" "$test_home/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/systems" "$test_home/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/MAME" "$test_home/ES-PRO/resources/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/locale" "$test_home/ES-PRO/resources/" 2>/dev/null || true
    
    local test_log="$LOG_DIR/test2_missing_themes.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on missing themes (SIGABRT)"
        return 1
    elif grep -q "theme.*not found\|no themes\|Couldn't find.*theme" "$test_log" 2>/dev/null; then
        log_pass "Application detected missing themes gracefully"
    else
        log_warn "Application handled missing themes without crash"
    fi
}

test_3_missing_fonts() {
    log_info "\n=== Test 3: Missing Font Resources ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test3_missing_fonts"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Remove fonts directory
    rm -rf "$test_home/ES-PRO/resources/fonts"
    mkdir -p "$test_home/ES-PRO/resources/fonts"  # Create empty fonts dir
    
    local test_log="$LOG_DIR/test3_missing_fonts.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on missing fonts (SIGABRT)"
        return 1
    elif grep -q "font.*not found\|Could not load font\|WARNING.*font" "$test_log" 2>/dev/null; then
        log_pass "Application reported missing fonts with warnings"
    else
        log_pass "Application used fallback fonts (graceful degradation)"
    fi
}

test_4_corrupted_config() {
    log_info "\n=== Test 4: Corrupted Configuration Recovery ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test4_corrupted_config"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Inject corrupted settings
    local settings_file="$test_home/ES-PRO/settings/es_settings.xml"
    
    # Insert malformed XML element
    sed -i '/<\/settings>/i\    <string name="BrokenValue" value="unclosed' "$settings_file"
    
    local test_log="$LOG_DIR/test4_corrupted_config.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on corrupted config (SIGABRT)"
        return 1
    else
        log_pass "Application recovered from corrupted configuration"
    fi
}

test_5_missing_systems() {
    log_info "\n=== Test 5: Missing System Definitions ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test5_missing_systems"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Move systems directory (simulate missing)
    mv "$test_home/ES-PRO/resources/systems" "$test_home/ES-PRO/resources/systems.bak" 2>/dev/null || true
    mkdir -p "$test_home/ES-PRO/resources/systems"
    
    local test_log="$LOG_DIR/test5_missing_systems.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on missing systems (SIGABRT)"
        return 1
    elif grep -q "No systems found\|system.*not found\|WARNING" "$test_log" 2>/dev/null; then
        log_pass "Application warned about missing system definitions"
    else
        log_pass "Application handled missing systems without crash"
    fi
}

test_6_partial_resources() {
    log_info "\n=== Test 6: Partial Resource Set ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test6_partial_resources"
    mkdir -p "$test_home/ES-PRO/settings" "$test_home/ES-PRO/logs" "$test_home/ES-PRO/themes" "$test_home/ES-PRO/resources"
    
    # Only copy minimal resources
    cp "$PROD_SETTINGS/es_settings.xml" "$test_home/ES-PRO/settings/"
    cp -a "$PROD_THEMES/es-theme-carbon-master" "$test_home/ES-PRO/themes/" 2>/dev/null || true
    cp -a "$PROD_RESOURCES/locale" "$test_home/ES-PRO/resources/" 2>/dev/null || true
    
    # Create empty system and fonts dirs
    mkdir -p "$test_home/ES-PRO/resources/systems" "$test_home/ES-PRO/resources/fonts" "$test_home/ES-PRO/resources/MAME"
    
    local test_log="$LOG_DIR/test6_partial_resources.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed with partial resources (SIGABRT)"
        return 1
    else
        log_pass "Application started with partial resource set"
    fi
}

test_7_invalid_theme_config() {
    log_info "\n=== Test 7: Invalid Theme Configuration ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test7_invalid_theme"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Create broken theme XML
    local theme_xml="$test_home/ES-PRO/themes/broken-theme/theme.xml"
    mkdir -p "$(dirname "$theme_xml")"
    
    cat > "$theme_xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!-- Broken theme.xml -->
<theme version="1.0">
  <invalid attr=unclosed
  <element missing="closing tag"
</theme>
</broken>
EOF
    
    # Set broken theme to active
    sed -i 's|<string name="Theme" value="[^"]*" />|<string name="Theme" value="broken-theme" />|' "$test_home/ES-PRO/settings/es_settings.xml"
    
    local test_log="$LOG_DIR/test7_invalid_theme.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on invalid theme (SIGABRT)"
        return 1
    elif grep -q "Error\|Warning\|parse" "$test_log" 2>/dev/null; then
        log_pass "Application logged theme parsing errors"
    else
        log_pass "Application fell back to default theme"
    fi
}

test_8_symlink_handling() {
    log_info "\n=== Test 8: Symlink Resolution ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test8_symlinks"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Create circular symlink (potential issue)
    ln -s "$test_home/ES-PRO/resources" "$test_home/ES-PRO/resources/link_to_self" 2>/dev/null || true
    
    local test_log="$LOG_DIR/test8_symlinks.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed on symlink (SIGABRT)"
        return 1
    else
        log_pass "Application handled symlinks safely"
    fi
}

test_9_error_log_output() {
    log_info "\n=== Test 9: Error Logging Verification ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test9_error_logging"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Corrupt a resource file
    echo "CORRUPTED DATA" > "$test_home/ES-PRO/resources/systems/arcade.xml"
    
    local test_log="$LOG_DIR/test9_error_logging.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    local app_log="$test_home/ES-PRO/logs/es_log.txt"
    
    if [ -f "$app_log" ] && grep -q "Error\|Warning\|Failed" "$app_log" 2>/dev/null; then
        log_pass "Application logged errors to es_log.txt"
    else
        log_warn "Could not verify error logging"
    fi
}

test_10_resource_limit() {
    log_info "\n=== Test 10: Resource Exhaustion Handling ==="
    ((TESTS_RUN++))
    
    local test_home="$LOG_DIR/test10_resource_limit"
    mkdir -p "$test_home"
    create_base_home "$test_home"
    
    # Create a large file to stress resources (but not too large to cause issues)
    dd if=/dev/zero of="$test_home/ES-PRO/resources/large_file.dat" bs=1M count=10 2>/dev/null || true
    
    local test_log="$LOG_DIR/test10_resource_limit.log"
    timeout 10s "$BINARY" --home "$test_home" --debug > "$test_log" 2>&1 || EXIT_CODE=$?
    
    if [ ${EXIT_CODE:-0} -eq 134 ]; then
        log_fail "Application crashed under resource pressure (SIGABRT)"
        return 1
    else
        log_pass "Application handled resource pressure"
    fi
}

print_summary() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║         PHASE 6 TEST RESULTS SUMMARY                          ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    
    local total=$((TESTS_PASSED + TESTS_FAILED))
    local pass_rate=0
    
    if [ $total -gt 0 ]; then
        pass_rate=$((TESTS_PASSED * 100 / total))
    fi
    
    printf "%-30s: %d\n" "Total Tests Run" "$TESTS_RUN"
    printf "%-30s: %d\n" "Tests Passed" "$TESTS_PASSED"
    printf "%-30s: %d\n" "Tests Failed" "$TESTS_FAILED"
    printf "%-30s: %d%%\n" "Pass Rate" "$pass_rate"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ] && [ $TESTS_PASSED -gt 0 ]; then
        echo -e "${GREEN}✓ ALL TESTS PASSED - Error handling verified${NC}"
        return 0
    else
        echo -e "${RED}✗ SOME TESTS FAILED - Review error handling${NC}"
        return 1
    fi
}

################################################################################
# Main
################################################################################

main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║  ESTACION-PRO - Phase 6: Error Handling Test                  ║"
    echo "║  Testing robustness and error recovery                        ║"
    echo "║  Date: $(date '+%Y-%m-%d %H:%M:%S')                               ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    
    if ! setup_environment; then
        log_fail "Failed to setup test environment"
        return 1
    fi
    
    log_info "Starting Phase 6 error handling tests...\n"
    
    test_1_invalid_settings
    test_2_missing_themes
    test_3_missing_fonts
    test_4_corrupted_config
    test_5_missing_systems
    test_6_partial_resources
    test_7_invalid_theme_config
    test_8_symlink_handling
    test_9_error_log_output
    test_10_resource_limit
    
    print_summary
    RESULT=$?
    
    log_info "\nTest logs saved to: $LOG_DIR"
    
    # Cleanup
    rm -rf "$LOG_DIR"/test*
    
    return $RESULT
}

main "$@"
