aufs
=========

aufs - simple Linux kernel file system for SPbAU (http://mit.spbau.ru/) OS course.

## How to format
```
mkfs.aufs /dev/nvme0n1
```

## make kern
```
sudo rmmod aufs && make clean && make -j && sudo insmod aufs.ko
```

## make user
```
sudo blkdiscard /dev/nvme0n1
make clean && make -j && sudo ./mkfs.aufs /dev/nvme0n1
```

Here is the reference output of mkfs in default settings. Due to the changes in code and parameters settings, the real output may be deviated from this reference output.

```
num_block_zone_map*block_size*8(32768) n_zones(31256725)
NumZones() 32768
BLOCK SIZE 4096 , ZoneSize(): 16384 in CountInodeBLocks: 
inode blocks: 512
inode blocks: 512
fill inode block: 3 index: 1oofset: 64
inode blocks: 512
root inode no: 1
root inode first block no: 519
root inode zone_size: 16384
root inode block_size: 4096
```

## How to mount
```
sudo mount /dev/nvme0n1 ~/aufsMountPoint
sudo umount ~/aufsMountPoint/
```

## How to test
This test assumes the nvme disk is already mounted before test.

In test/, execute

```
dmesg >../logs/dmesg.log && sudo `which python` MultiBlockFSTest.py >../logs/MultiBlockFSTest.log && dmesg>../logs/dmesg.end.log
···