#!/bin/sh

# https://gist.github.com/tansy/c4a1f7ea1019915b4263c024757d3ac4
# https://github.com/jinfeihan57/p7zip/pull/186#issuecomment-1223976885

if [ $# -eq 0 ]; then
    echo "d2u-dir <directory1/dos> <directory2/ux>"
    echo "will copy contents of <dir1> to <dir2> changing new lines from dos to unix."
    echo "It ignores binary files by their extension,"
    echo "so you may want to copy whole dir1 to dir2 first and then apply d2u-dir."
    exit 0
fi

# reference dir:
DIR0="${1}"
# new dir:
DIR1="${2}"
if [ -d "${DIR1}" ]; then
    echo "directory ${DIR1} exists, do you want to continue? [y/N]"
    read ANS
    if [ "${ANS}" == "y" ] || [ "${ANS}" == "Y" ]; then
        continue
    else
        exit 0
    fi
fi

IFS="
"

if [ -n "${DIR1}" ]; then

for line in `find "${DIR0}" -type d`; do
    mkdir -p "${DIR1}/${line#$DIR0}"
done
# every `.' denoting extension has to escaped, every extension terminated with $
EXCL_EXT='\.a|\.bmp$|\.bin$|\.chm$|\.cur$|\.dsp$|\.dsw$|\.exe$|\.gif$|\.icns$|\.ico$|\.jar$|\.jpg$|\.jpeg$|\.lib|\.pcx$$|\.png$|\.res$|\.sfx$|\.wav$'
for line in `find "${DIR0}" -type f | grep -iEv "$EXCL_EXT"`; do
    echo $line
    dos2unix < "${line}" > "${DIR1}/${line#$DIR0}" || cat < "${line}" > "${DIR1}/${line#$DIR0}"
    touch -r "${line}" "${DIR1}/${line#$DIR0}"
done
for line in `find "${DIR0}" -type f | grep -iE "$EXCL_EXT"`; do
    cat < "${line}" > "${DIR1}/${line#$DIR0}"
    touch -r "${line}" "${DIR1}/${line#$DIR0}"
done
for line in `find "${DIR0}" -type d`; do
    touch -r "${line}" "${DIR1}/${line#$DIR0}"
done

else

for line in `find "${DIR0}" -type f | grep -iEv "$EXCL_EXT"`; do
    dos2unix "${line}"
done

fi
