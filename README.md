# Emojicode [![Build Status](https://travis-ci.org/emojicode/emojicode.svg?branch=master)](https://travis-ci.org/emojicode/emojicode) [![Join the chat at https://gitter.im/emojicode/emojicode](https://badges.gitter.im/emojicode/emojicode.svg)](https://gitter.im/emojicode/emojicode?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

http://www.emojicode.org

Emojicode is an open source, high-level, multi-paradigm, object-oriented
programming language consisting of emojis.

## The Language

**To learn more about the language Emojicode visit http://www.emojicode.org/docs.**

## Installing

**You can easily install Emojicode from our stable prebuilt binaries: http://www.emojicode.org/docs/guides/install.html.**

## Building from source

If you need to build Emojicode from source you can of course also do this. **We, however, recommend you to install Emojicode from the prebuilt binaries if possible.**

Prerequisites:
- clang and clang++ 3.4 or newer, or
- gcc and g++ 4.8 or newer
- GNU Make
- SDL2 (libsdl2-dev) to compile the SDL package
  - `sudo apt-get install libsdl2-dev` on Debian/Ubuntu
  - `brew install SDL2` on OS X

Steps:

1. Clone Emojicode (or download the source code and extract it) and navigate into it:
   
   ```
   git clone https://github.com/emojicode/emojicode
   cd emojicode
   ```
   
    Beware of, that the master branch contains development code which probably contains bugs. If you want to build the latest stable release make sure to check it out first: `git checkout  v0.2.0-beta.3`
    
2.  Then simply run

  ```
  make
  ```

  to compile the Engine, the compiler and all default packages.

  You may need to use a smaller heap size on older Raspberry Pis. You can
  specify the heap size in bytes when compiling the engine:

  ```
  make HEAP_SIZE=128000000
  ```

  The default heap size is 512MB.

3. You can now either install Emojicode and run the tests:

   ```
   [sudo] make install && make tests
   ```
   
   or package the binaries for distribution:
   
   ```
   make dist
   ```
   
  After the command is done you will find a directory and a tarfile
in `builds` named after your platform, e.g. `Emojicode-0.2.0-beta.3-x86_64-linux-gnu`.

## Staying up to date

You can follow [@idmean](https://twitter.com/idmean) (the creator of Emojicode) on Twitter to stay up to date.

## Contributions

Contributions are welcome! A contribution guideline will be setup soon.

If you wan't to help and have no idea how, check out the [issues](https://github.com/emojicode/emojicode/issues) or ask in the [Gitter Chat](https://gitter.im/emojicode/emojicode).
