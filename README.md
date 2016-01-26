Quiet
===========

This library uses liquid SDR to encode and decode data through 44.1kHz. This makes it suitable for sending data across a 3.5mm headphone jack from one device to another. It can also be built using emscripten to generate a .js file which can do transmission and reception from the browser.

Building this project on a modern computer is somewhat tricky as libfec does not know about 64-bit architectures. I have a customized version which can handle this case which should be coming soon.

Generating the emscripten build requires that all dependencies be built with emscripten (e.g. emconfigure ./configure; emmake make; make install) for each dependency. After the emscripten-compiled dependencies are installed, run this project's ./build_js. Alternately, you may use the precompiled encode.js provided by this project. Be sure to include the third party license information in web/LICENSE-3RD-PARTY.txt.

Profiles
-----------
The encoding and decoding process are controlled by the profiles in `web/profiles.json`. Each profile contains a complete set of parameters such as modem type and error correction. This project uses libjansson to read the profiles and then select one based on a given profile name.

Ultrasonic
-----------
The `highfreq` profile encodes data into a very low bitrate, but the audio content falls entirely between 18.5kHz and 19.5kHz, which should pass through audio equipment relatively well while being inaudible to the average person. This is a good option for sending data through a channel where you would prefer not to disrupt or notify human listeners.

Dependencies
-----------
* [Liquid DSP](https://github.com/jgaeddert/liquid-dsp)
* [libfec](http://www.ka9q.net/code/fec/)
* [Jansson](https://github.com/akheron/jansson)
