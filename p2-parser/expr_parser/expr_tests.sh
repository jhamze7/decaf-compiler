#!/bin/bash

make

echo ""

declare -a VALID_TEST_CASES=("1" "1+2" "1+2+3" "2*3" "2*3*4" "2+3*4" "2*3+4"
                             "(2)" "2*(3+4)" "((4+2))")
echo "== VALID =="
for t in "${VALID_TEST_CASES[@]}"; do
    echo -n "$t: "
    ./expr_parser "$t"
done

echo ""

declare -a INVALID_TEST_CASES=("" "1+" "+" "4**5" "4+*5" "(" "()" "((3)" "(3))")
echo "== INVALID =="
for t in "${INVALID_TEST_CASES[@]}"; do
    echo -n "$t: "
    ./expr_parser "$t"
done

echo ""

exit 0
