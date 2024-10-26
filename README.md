# btccpu
Useless bitcoin cpu miner using the CKPool stratum server.

This is just a project for my education. I believe the only way to know how
something works is if you can implement it in code. In this case, C++20. I've
been using ChatGPT to try and "teach" me the stratum protocol as implemented
by CKPool.

I have Bitcoin Core and CKPool running on a Raspbery Pi 5. In turn, there is
a BitAxe Supra miner connected to the Pi5 via the LAN to do lottery mining.
Think of the Pi5 as running a small, private bitcoin mining pool.

Everything is hard coded in my C++ code as I type this. It is being
developed on an M2 Mac mini with Homebrew installed. It uses the following
two packages:

```
boost
openssl```

They can be installed via homebrew like so:


```brew install boost openssl```

The build.sh script is for compiling on the M2 Mac mini. A linux machine would
likely require a slightly different build command. An Intel Mac probably will also
as brew's package management varies between ARM64 and Intel architectures.

