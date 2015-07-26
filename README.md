# MagmaFS

Magma is a **distributed network filesystem** for Linux. By distributed we mean that 
**Magma spreads data across a cluster** (called a *lava network*) of nodes (called *volcanos*). 
Each volcano holds a *slice of the SHA1 keyspace*. When a file is stored inside Magma, its 
path is hashed to produce an SHA1 key. The volcano node that holds the slice of the SHA1 
keyspace that the key belongs to will host the file.

This design has been conceived to *overcome the well known problems bound to the presence of
single point of failure roles* like master nodes, name nodes and so on.

**Magma is currently an experimental project**. It's able to form a network of two or possibly
more nodes and to redundantly copy each operation to the next node (on a two volcanos scenario
this is basically equivalent to a network RAID1). Magma has a partially implemented load
balancer that will move keys across nodes to equalize the pressure on the whole network.

A Magma filesystem can be mounted on Linux using the provided `mount.magma` command. It requires
*FUSE support* in the kernel.

Magma uses UDP datagram for its Flare (data) and Node (metadata) protocols, respectively defaulting
on ports 12000 and 12001. On port 12002 it features a TCP console which can be reached with telnet.

A Magma network can be already built, shutdown and re-established, but **no extensive testing has
been done on its reliability**.

## How to compile

Magma is divided in four packages:

 * libmagma
 * magmad
 * mount.magma
 * magma_tools
 
*libmagma* is the foundation library used by the other three packages and must be compiled first.
*magmad* is the Magma daemon. A lava network is formed by several *magmad*. *mount.magma* runs on each client and mounts the lava network on a local directory. 

Magma uses the usual autotools setup:

    $ ./autogen.sh
    $ ./configure
    $ make
    $ sudo make install

## How to build a network

A lava network is build by starting the first *magmad* with the `-b` option. For example this command:

    $ magmad -n wallace -s wallace.intranet -k m4gm4s3cr3t -i 192.168.1.20 -b -d /srv/magma -Dfs

starts a lava network (-b) on the host called wallace (fully qualified name: wallace.intranet) using secret key `m4gm4s3cr3t` using address 192.168.1.20, storing data in `/srv/magma` and enabling logging for the flare operations (-Df) and the SQL queries used to store metadata (-Ds).

