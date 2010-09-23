#!/bin/bash

function mkfs()
{
    dd if=/dev/zero of=$1.img bs=$[1024*1024] count=$[300]
    /sbin/mkfs.ext3 $1.img
}

function usage()
{
    echo "usage: $@ [ find | read | write ] "
}

if [ -z "$1" ]; then
    usage
    exit
fi

if [ "$1" = "write" ]; then
    mkfs $1
elif [ "$1" = "read" ]; then
    mkfs $1
    mount $1.img /mnt/tmp -o loop
    dd if=/dev/zero of=/mnt/tmp/a bs=$[1024*1024] count=128
    umount /mnt/tmp
elif [ "$1" = "find" ]; then
    sf=32
    mkfs $1
    mount $1.img /mnt/tmp -o loop
    for((i=0; i<$sf; i++)); do
	mkdir /mnt/tmp/$i; cd /mnt/tmp/$i;
	for((j=0; j<$sf; j++)); do
	    touch $j;
	done
	cd -
    done
    umount /mnt/tmp
else
    usage
fi
