#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'linux' ]]; then
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
    sudo apt-get update
    sudo apt-get install -y gcc-4.9 g++-4.9
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.9
    sudo apt-get install -y libasound-dev alsa-utils
fi
