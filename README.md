# Pigmy, Big Brother watches for you!

# Be a Free Giant - use a local `i2pchat` app! Edit the future.

i2pchat
 1. a lot of fixes needed;
 2. https://github.com/vituperative/i2pchat has updated version with many fixes and unknown security level (Hypnosis had already spotted some security holes at Vituperative's i2pchat and reported them, these were fixed);
 3. i2pchat by vade AKA i2pchat.i2p is a completely different unrelated piece of software


# I2PChat

## Screenshots

![screenshot-roster](https://user-images.githubusercontent.com/19966907/82762245-7cf1db80-9e32-11ea-9c0c-5e711e6ff5f9.png) ![screenshot-file-transfer](https://user-images.githubusercontent.com/19966907/82762322-e1149f80-9e32-11ea-80dc-762adcbcac48.png)


## Features

 * The communication goes «directly over i2p» from client to client, no server is required.[1] Note: The connections to the so-called «seedless» servers were removed from this project ; see : https://github.com/nonlin-lin-chaos-order-etc-etal/i2pchat/issues/40 . —hypn.
 
## How to run it

You need to enable SAM in your router on <a href="http://127.0.0.1:7657/configclients">java i2p configclients page</a> or i2pd's i2pd.conf [sam] section to make I2P Chat work over your I2P router.

## Project status, news and history

### Project status

Now the development was resumed by Hypnosis-i2p and R4SAS, with a lot more folks as testers &amp; bugs+issues reporters.

### Current news

5 Jan, 2017: Original repo at http://git.repo.i2p/w/I2P-Messenger-QT.git was fully merged here.
  
### History

Original developer of this messenger went away.

## License

The license of this software is GPLv2.

### Build instructions

 * Install prerequisites:

At Linux Mint Codename Gigi (or Debian 13(?)), use

```sh
sudo apt update && sudo apt upgrade && sudo apt dist-upgrade && sudo apt-get install qt5-qmake build-essential libqt5multimedia5 qtmultimedia5-dev libqt5widgets5t64
```

At other Debian-based operating systems, try:

```sh
sudo apt update && sudo apt upgrade && sudo apt dist-upgrade && sudo apt-get install qt5-qmake qt5-default build-essential libqt5multimedia5 qtmultimedia5-dev
```

 * Run qmake for either release:
```sh
qmake I2P-Messenger.pro "CONFIG += release"
```
or for debug:
```sh
qmake I2P-Messenger.pro "CONFIG += debug"
```
 * And after that,
```
make -j `nproc`
```
or simply
```
make
```

## Running

On Linux, `make` creates `I2P-Messenger` executable in the current folder. Run it with `./I2P-Messenger`. When ran, switch yourself to online. It will generate your Destination address (a key) on first connect to SAM.

## Old files and sites

 * http://echelon.i2p/qti2pmessenger/macos → Mac OS X versions, not tested;
 * http://echelon.i2p/qti2pmessenger/ → old eche|on's site for I2P Chat (versions up to 0.2.24);
 * https://launchpad.net/~i2p.packages/+archive/ubuntu/i2p/+sourcepub/4824794/+listing-archive-extra → Kill Your TV's PPA (v.0.2.25).
 * http://git.repo.i2p/w/I2P-Messenger-QT.git/shortlog → original repo (fully merged into this /i2pchat)
 
## Forum thread (English)

http://forum.i2p/viewtopic.php?t=2474

## List of relevant repositories (in Russian)
 
я два репа на гитхабе создал
 * один для антиквариата по части и2п массажника https://github.com/i2pchat/I2PQtMessengerLegacy ;
 * другой для разработки https://github.com/i2pchat/i2pchat .

## Footnotes

[1] http://forum.i2p/viewtopic.php?p=11071#11071
