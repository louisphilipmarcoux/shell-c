#!/bin/bash
# Integration test suite for shell-c
set -u

SHELL_BIN="${1:-./build/shell}"
PASS=0
FAIL=0
TOTAL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
BOLD='\033[1m'
NC='\033[0m'

run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    TOTAL=$((TOTAL + 1))

    actual=$("$SHELL_BIN" -c "$input" 2>/dev/null)
    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Input:    $input"
        echo "  Expected: $expected"
        echo "  Got:      $actual"
    fi
}

run_test_exit() {
    local name="$1"
    local input="$2"
    local expected_exit="$3"
    TOTAL=$((TOTAL + 1))

    "$SHELL_BIN" -c "$input" >/dev/null 2>&1
    local actual_exit=$?
    if [ "$actual_exit" -eq "$expected_exit" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Input:         $input"
        echo "  Expected exit: $expected_exit"
        echo "  Got exit:      $actual_exit"
    fi
}

run_test_stderr() {
    local name="$1"
    local input="$2"
    local expected_pattern="$3"
    TOTAL=$((TOTAL + 1))

    actual=$("$SHELL_BIN" -c "$input" 2>&1 >/dev/null)
    if echo "$actual" | grep -q "$expected_pattern"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Input:    $input"
        echo "  Pattern:  $expected_pattern"
        echo "  Stderr:   $actual"
    fi
}

echo -e "${BOLD}Running shell-c integration tests...${NC}"
echo

# --- Builtins ---
echo -e "${BOLD}Builtins${NC}"
run_test "echo basic"           'echo hello'            "hello"
run_test "echo multiple args"   'echo hello world'      "hello world"
run_test "echo empty"           'echo'                  ""
run_test "pwd"                  'pwd'                   "$(pwd)"
run_test "type builtin"         'type echo'             "echo is a shell builtin"
run_test "type external"        'type ls'               "ls is /usr/bin/ls"
run_test_stderr "type not found" 'type nonexistent_cmd_xyz' ""
run_test "cd and pwd"           'cd /tmp; pwd'          "/tmp"
run_test "cd HOME"              'cd; pwd'               "$HOME"
run_test_exit "exit 0"          'exit 0'                0
run_test_exit "exit 42"         'exit 42'               42
run_test_exit "exit default"    'exit'                  0

# --- External commands ---
echo -e "${BOLD}External commands${NC}"
run_test "external ls"          'ls /dev/null'          "/dev/null"
run_test "external with args"   'basename /usr/bin/ls'  "ls"
run_test_exit "false exit"      'false'                 1
run_test_exit "true exit"       'true'                  0

# --- Pipes ---
echo -e "${BOLD}Pipes${NC}"
run_test "simple pipe"          'echo hello | cat'      "hello"
run_test "multi pipe"           'echo hello | cat | cat' "hello"
run_test "pipe with wc"         'echo "one two three" | wc -w' "3"
run_test "pipe with grep"       'echo hello | grep hello' "hello"

# --- Redirections ---
echo -e "${BOLD}Redirections${NC}"
TMPDIR=$(mktemp -d)

run_test "output redirect"      "echo test_output > $TMPDIR/out; cat $TMPDIR/out" "test_output"
run_test "append redirect"      "echo line1 > $TMPDIR/append; echo line2 >> $TMPDIR/append; cat $TMPDIR/append" "line1
line2"

echo "input_test_data" > "$TMPDIR/input"
run_test "input redirect"       "cat < $TMPDIR/input"   "input_test_data"

run_test "stderr redirect"      "ls /nonexistent 2> $TMPDIR/err; cat $TMPDIR/err" "$(ls /nonexistent 2>&1)"
run_test_stderr "bad redirect"  "echo x > /nonexistent/path" "No such file"

rm -rf "$TMPDIR"

# --- Logical operators ---
echo -e "${BOLD}Logical operators${NC}"
run_test "and success"          'true && echo yes'          "yes"
run_test "and failure"          'false && echo no'          ""
run_test "or success"           'true || echo no'           ""
run_test "or failure"           'false || echo yes'         "yes"
run_test "chain and-or"         'false || true && echo ok'  "ok"
run_test "chain or-and"         'true && false || echo fb'  "fb"

# --- Semicolons ---
echo -e "${BOLD}Sequences${NC}"
run_test "semicolon"            'echo a; echo b'            "a
b"
run_test "semicolon three"      'echo a; echo b; echo c'    "a
b
c"
run_test "trailing semicolon"   'echo ok;'                  "ok"

# --- Variable expansion ---
echo -e "${BOLD}Variable expansion${NC}"
run_test "expand HOME"          'echo $HOME'            "$HOME"
run_test "expand USER"          'echo $USER'            "$USER"
run_test "expand braces"        'echo ${HOME}'          "$HOME"
run_test "expand exit status"   'false; echo $?'        "1"
run_test "expand exit 0"        'true; echo $?'         "0"
# PID test: just verify it outputs a number
TOTAL=$((TOTAL + 1))
pid_output=$("$SHELL_BIN" -c 'echo $$' 2>/dev/null)
if echo "$pid_output" | grep -qE '^[0-9]+$'; then
    PASS=$((PASS + 1))
else
    FAIL=$((FAIL + 1))
    echo -e "${RED}FAIL${NC}: expand PID - expected number, got: $pid_output"
fi
run_test "expand default"       'echo ${UNSET_VAR_XYZ:-fallback}' "fallback"
run_test "export and expand"    'export MY_TEST_VAR=hello; echo $MY_TEST_VAR' "hello"
run_test "unset var"            'export MY_TEST_VAR=hello; unset MY_TEST_VAR; echo ${MY_TEST_VAR:-gone}' "gone"

# --- Tilde expansion ---
echo -e "${BOLD}Tilde expansion${NC}"
run_test "tilde home"           'echo ~'                "$HOME"
run_test "tilde path"           'echo ~/foo'            "$HOME/foo"

# --- Quotes ---
echo -e "${BOLD}Quoting${NC}"
run_test "single quotes"        "echo 'hello world'"       "hello world"
run_test "double quotes"        'echo "hello world"'       "hello world"
run_test "single preserves $"   "echo '\$HOME'"            '$HOME'
run_test "double expands $"     'echo "$HOME"'             "$HOME"

# --- CLI flags ---
echo -e "${BOLD}CLI flags${NC}"
# Version flag test (not via -c, direct invocation)
TOTAL=$((TOTAL + 1))
version_output=$("$SHELL_BIN" --version 2>/dev/null)
if [ "$version_output" = "shell-c 0.1.0" ]; then
    PASS=$((PASS + 1))
else
    FAIL=$((FAIL + 1))
    echo -e "${RED}FAIL${NC}: version flag - expected 'shell-c 0.1.0', got: $version_output"
fi

# --- Summary ---
echo
echo -e "${BOLD}Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC} out of ${TOTAL} tests"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
