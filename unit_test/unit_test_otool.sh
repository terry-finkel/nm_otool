#!/bin/zsh
echo "\x1b[33;1mtests for otool, -t\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_otool -t $file > a1;
	otool -t $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for otool, -d and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_otool -d --arch all $file > a1;
	otool -d -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for otool, -h and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_otool -h --arch all $file > a1;
	otool -h -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for otool, -dht and all archs\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_otool -dht $file > a1;
	otool -dht $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

echo "\x1b[33;1mtests for otool, everything\x1b[0m";
for file in ./valid_binaries/*/*;
do;
	../ft_otool -dht --arch all $file > a1;
	otool -dht -arch all $file > a2;
	diff a1 a2 > result;
	if (( $? != 0 ))
		then echo "diff in file $file:";
	fi
done;

