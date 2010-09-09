#!/bin/sh

massage_input() {
    sed -e 's/#.*// # remove comments' \
	-e 's/^[ \t]\+//  # delete leading whitespace' \
	-e 's/[ \t]\+$//  # delete trailing whitespace' | \
	tr \\t ' ' | tr -s ' '
}

check_format() {
    local errline="$(massage_input <"$1" | \
	grep -En -m1 -v '^([0-9]+ [a-zA-Z0-9]+)?$' | cut -d: -f1)"

    if [ -n "$errline" ]; then
	echo "$1:$errline: invalid format of input line" >&2
	exit 1
    fi
}

WRAP_LINES=1
maybe_fold() {
    if [ -n "$WRAP_LINES" -a "$WRAP_LINES" != '0' ]; then
	tr \\n ' ' | fold -s -w70
    else
	cat
    fi
}

[ $# -eq 1 ] || { echo "Usage: ${0##*/} SOURCE_FILE" >&2; exit 1; }

SRC="$1"
check_format $SRC

TMP_CTT=`mktemp` TMP_SEQ=`mktemp`
trap "rm $TMP_CTT $TMP_SEQ" 0

massage_input <$SRC | grep -v '^$' >$TMP_CTT
[ -s $TMP_CTT ] || { echo "$SRC: no useful data" >&2; exit 1; }

cat <<EOF
/*
 * This file was generated from \`$SRC'
 * `date -R`
 */

/* A mapping of CallTransactionType (CTT) to its numeric value */
static struct {
	const char *symbol;
	uint8_t number;
} ctt_dict[] = {
	{ NULL, 0 },
EOF
sort -k2 $TMP_CTT | awk '{ printf("\t{ \"%s\", %d },\n", $2, $1) }'
echo '};'
echo

seq 0 $(tail -n1 $TMP_CTT | cut -d' ' -f1) | sort >$TMP_SEQ

echo "/* N-th element is an index of CTT=N in \`ctt_dict' */"
echo 'static uint8_t ctt_idx[] = {'
sort -k1,1 $TMP_CTT | join -a1 $TMP_SEQ - | sort -k2 | \
    awk '{ print $1, (NF > 1) ? ++i "," : "0," }' | sort -n -k1,1 | \
    cut -d' ' -f2 | head -c-2 | maybe_fold | sed 's/^/\t/'
echo
echo '};'
