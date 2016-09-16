#!/bin/bash
citefile=CITES.bib
backfile=CITES.bib.BaK

if [ -e "$citefile" ] ; then
	mv $citefile  $backfile
fi

# Concatenate all bibtex files to $citefile
cat *.bib >> $citefile

if [ "$?" == 0 ] ; then 
	echo "Successfully created '$citefile',  removing '$backfile'"
	rm -f $backfile
else
	echo "Error creating '$citefile',  restoring '$backfile'"
	mv $backfile  $citefile
fi
