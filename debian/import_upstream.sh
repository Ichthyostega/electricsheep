#!/bin/bash
#
# ElectricSheep: import an upstream tree from Git
#                and transform into a form suitable as 'upstream-tarball'

if [ ! -d client_generic ]
	then
	echo "should be in the root of a Git checkout. Expect directory 'client_generic'"
	exit -1
fi

cd client_generic
git rm -rf MacBuild ffmpeg boost libpng libxml lua5.1

cd ..
git mv client_generic/* .
rmdir client_generic

echo "...transformed source tree"
echo "please check in to Git"
