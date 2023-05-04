#!/bin/bash

(
echo "--------"
./test

echo "--------"
./test --int 12

echo "--------"
./test --none -m --int2 32 --long2 111 --flt 0.1 -g 94.222 --size_t 123

echo "--------"
./test -m --int 11 -l 333 --flt 0.09 --dbl 43.4 --dbl2 90.88 --str "a string"

echo "--------"
./test --str "a string" --str2 string2

echo "--------"
./test --str "a string" --iarr 1,2,3

echo "--------"
./test --str "a string" --iarr 3,4 --larr 123123,456456

echo "--------"
./test --none --iarr 1,1 --farr 1,3.1,4.e-6 --darr '1;2.;3.1;4E3' --sarr 3

echo "--------"
./test --iarr 1.1,1 --sarr 3e1

) >unittest.tmp 2>&1

diff unittest.tmp unittest.out
exit $?
