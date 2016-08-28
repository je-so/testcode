#!/bin/sh
font="/usr/share/fonts/truetype/droid/DroidSansMono.ttf"
nrchar=95 # 32 .. 126
echo Read $font
echo Export ASCII into test.png
#
# Compile C-File
#
gcc -std=c11 -owrite write_c_file.c || exit 1
#
# Generate font.png with fontforge
#
fontforge -lang=ff -script export.ff "$font" 2>/dev/null || fontforge -lang=ff -script export.ff "$font" || exit 1
size=`file font.png | cut -d , -f 2 -`
X=`echo $size | cut -d " " -f 1`
Y=`echo $size | cut -d " " -f 3`
charwidth=$(( X/nrchar ))
charheight=40
echo IMAGE-SIZE: $X x $Y
echo CHAR WIDTH: $charwidth
echo CHAR HEIGHT: $charheight
xoff=0
yoff=0
convert -depth 1 -depth 8 font.png font.gray
convert -depth 1 -depth 8 font.png font-sw.png
c_file="font_${charwidth}x${charheight}.c"
./write font.gray $X $Y $xoff $yoff $charwidth $charheight > $c_file
rm ./font.gray ./font.png ./write
