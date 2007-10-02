#! /bin/bash
# ---------------------------------------
# Written by Sebastian Pipping <sping@xiph.org>
#
# WORK IN PROGRESS!
# ---------------------------------------

IN_FILE="lib/cpu_asm_1_gcc.c"
OUT_FILE="lib/cpu_asm_1_msvc.c"

echo "#ifdef USE_ASM" > ${OUT_FILE}
echo "" >> ${OUT_FILE}
echo "__asm {" >> ${OUT_FILE}

cat ${IN_FILE}\
    | grep '\\n\\t'\
    | sed -r 's/\\n\\t//'\
    | sed -r 's/%0/eax/'\
    | sed -r 's/%1/ebx/'\
    | sed -r 's/"(.+)"/\1/'\
    | sed -r 's/\$((0x)?[0-9]+)/\1/'\
    >> lib/cpu_asm_1_msvc.c

echo "}" >> ${OUT_FILE}
echo "" >> ${OUT_FILE}
echo "#endif /* USE_ASM */" >> ${OUT_FILE}

cat ${OUT_FILE}
