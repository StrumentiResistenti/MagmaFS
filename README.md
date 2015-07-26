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

## How to expand a network

A lava network can be expanded by starting more nodes in *join mode*, passing to option `-r` the IP address of the node to be contacted for the join operation. For example:

  $ magmad -n gromit -s gromit.intranet -k m4gm4s3cr3t -i 192.168.1.21 -d /srv/magma -r 192.168.1.20 -DV

`magmad` is started on a node called gromit on IP address 192.168.1.21 and is instructed to contact node at 192.168.1.20 (the previously started wallace node) to join the network after it. The remote node will guess the joining node key slice by taking its last unused key and committing it to the joining node. For example, being wallace the only node of the network, its key slice is the whole key space, from `0000000000000000000000000000000000000000` to `ffffffffffffffffffffffffffffffffffffffff`. It's last saved key is `8363a82ab9833cd88d90382b3984e834a83v23ab`, so it entrust the key slice from `8363a82ab9833cd88d90382b3984e834a83v23ac` to `ffffffffffffffffffffffffffffffffffffffff`.

Joining node receives a copy of all the keys managed by the joined node and starts acting as a redundancy mirror of it. When a third node, called penguin, joins the network the redundancy topology will be:

| keys of  | are replicated on |
| -------- | ------------------|
| wallace  | gromit            |
| gromit   | penguin           |
| penguin  | wallace           |

## How to shutdown a network

Using `telnet`, connect to `magmad` console port and issue a `shutdown`:

    $ telnet 192.168.1.20 12002
    Trying 192.168.1.20...
    Connected to 192.168.1.20.
    Escape character is '^]'.

    Welcome to MAGMA version 0.1.20150213
    This is server wallace (192.168.1.20)
    Type help [ENTER] for available commands.

    MAGMA [wallace]:/> shutdown
    Shutting down magma... Connection closed by foreign host.
    $

## How to restart a previously shutdown network

To restart a network just run each node without any `-b` or `-r` option. Lava nodes will reload their previous configuration from disk and will wait for the whole network to be available. The node that holds the key of a special internal directory called `/.dht/` will act as coordinator.

## How to mount a Lava network

`mount.magma` is the client side tool that mounts a Lava network on a local directory. The syntax is:

    $ mount.magma --host=192.168.1.20 -s --key=m4gm4s3cr3t ~/magma

The mountpoint is specified as the last argument. Every volcano can be contacted to mount the lava network. The mount protocol will evolve to download the whole network topology so a client can directly request a key to the hosting node instead of requesting every key to the node received with the `--host` argument.

## Routing and proxying



