#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/nbx.png
ICON_DST=../../src/qt/res/icons/nbx.ico
convert ${ICON_SRC} -resize 16x16 nbx-16.png
convert ${ICON_SRC} -resize 32x32 nbx-32.png
convert ${ICON_SRC} -resize 48x48 nbx-48.png
convert nbx-16.png nbx-32.png nbx-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/nbx_testnet.png
ICON_DST=../../src/qt/res/icons/nbx_testnet.ico
convert ${ICON_SRC} -resize 16x16 nbx-16.png
convert ${ICON_SRC} -resize 32x32 nbx-32.png
convert ${ICON_SRC} -resize 48x48 nbx-48.png
convert nbx-16.png nbx-32.png nbx-48.png ${ICON_DST}
rm nbx-16.png nbx-32.png nbx-48.png
