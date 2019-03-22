#!/bin/zsh
echo "\x1b[33;1mtests for nm, no arguments\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_nm $file > a1;
	nm $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for nm, -rn and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_nm -rn --arch all $file > a1;
	nm -rn -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for nm, -pa and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_nm -pa --arch all $file > a1;
	nm -pa -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for nm, -u and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_nm -u --arch all $file > a1;
	nm -u -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for nm, -U and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_nm -U --arch all $file > a1;
	nm -U -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

