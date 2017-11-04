# Unacloud Task Scheduler

Process scheduler for the linux kernel (4.2.0 generic version) that focuses on managing either Desktop Grid or Volunteered Computing tasks. It that guarantees the tasks it manages won't increase the execution times of user tasks by more than a predefined threshold (percentage).

The policy used by the scheduler was defined in the research publication: Harvesting idle CPU resources for desktop grid computing while limiting the slowdown generated to end-users. Cluster Computing, Springer, Volume 18: 1331-1350.

## Compilation

To compile the kernel, you can use the `unacloud.sh` provided script.

```
./unacloud.sh deps      # install dependencies (only the first time)
./unacloud.sh control   # create control files for `.deb` installer
./unacloud.sh compile   # compile the kernel     
```

If you made some changes to the kernel, and modified the `debian/changelog`, you may need to regenerate the control files.

```
./unacloud.sh clean  
./unacloud.sh control
./unacloud.sh compile     
```

## Installation

The compilation process creates a set of `.deb` files in the parent folder. These files can be used to install the kernel.

```
sudo dpkg -i ../*.deb
```

You can also use the provided scripts to install the generated packages.

```
./unacloud.sh install
```


