aufs
=========

aufs - simple Linux kernel file system for SPbAU (http://mit.spbau.ru/) OS course.

** How to format
mkfs.aufs /dev/nvme0n1

** make kern
sudo rmmod aufs && make clean && make -j && sudo insmod aufs.ko

** make user
make clean && make -j && sudo ./mkfs.aufs /dev/nvme0n1

** How to mount
sudo mount /dev/nvme0n1 ~/aufsMountPoint
sudo umount ~/aufsMountPoint/