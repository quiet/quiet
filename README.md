<img src="https://quiet.github.io/qmp_header_t.png" alt="Quiet Modem Project logo" height="140" >

[Quiet](https://github.com/quiet/quiet/)
===========
[![Build Status](https://travis-ci.org/quiet/quiet.svg?branch=master)](https://travis-ci.org/quiet/quiet)


**[Documentation](http://quiet.github.io/quiet/)**


This library uses liquid SDR to transmit data through sound. This makes it suitable for sending data across a 3.5mm headphone jack or via speaker and mic. Quiet can build standalone binaries for encoding/decoding data via .wav files or for streaming through your soundcard via PortAudio. It can also be built as a library to be consumed by other C programs.

Build
-----------

With the [dependencies](#dependencies) installed, run `./bootstrap` provided by this library, which will create a `build` directory and invoke cmake. If you would like to install it after building, do `cd build && make install`.

Profiles
-----------
The encoding and decoding processes are controlled by the profiles in `quiet-profiles.json`. Each profile contains a complete set of parameters such as modem type and error correction.

### Cable
For cable transmission, use the `cable-` profiles. These will attempt to use nearly the full spectrum provided by your soundcard. As such, they contain many audible frequencies and should not be used on channels that could be heard by people.

### Ultrasonic
The `ultrasonic-` profiles encode data through a very low bitrate, but the audio content lies above 16kHz, which should pass through audio equipment relatively well while being inaudible to the average person. This is a good option for sending data through a channel where you would prefer not to disrupt human listeners.

Dependencies
-----------
* [Liquid DSP](https://github.com/quiet/liquid-dsp/tree/devel) Be sure to work from the devel branch
* [libfec](https://github.com/quiet/libfec) (optional but strongly recommended)
* [Jansson](https://github.com/akheron/jansson)
* [libsndfile](http://www.mega-nerd.com/libsndfile/) (optional)
* [PortAudio](http://www.portaudio.com/) (optional)

Acknowledgements
-----------
I'd like to thank the people who provided feedback and helped me with pull requests and advice on software

* Joseph Gaeddert, for his excellent SDR library, encouragement, and feedback on all things DSP
* Alon Zakai and @juj for advising me on emscripten and for taking my PRs
* Jan-Ivar Bruaroey and Maire Reavy for helping me patch the echo cancellation behavior of Firefox's getUserMedia
* Josh Gao for advising me on API design and C idioms.
* Ryan Hitchman for extensive testing on Android Chrome
* Fabrice Bellard for thoughtfully answering a stranger's question out of the blue about digital communications
