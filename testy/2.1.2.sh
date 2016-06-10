#!/bin/bash

echo "poczatkowy wynik:"
przed=`./rso-client list`
echo $przed
echo "Wylacz jeden z serwerow danych i wcisnij Enter"
read _ </dev/tty
echo "koncowy wynik:"
po=`./rso-client list`
echo $po
if test "$przed" == "$po"
then
	echo "wynik sie zgadza, test zaliczony"
else
	echo "wynik sie nie zgadza, test niezaliczony"
fi
