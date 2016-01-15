Quiet
===========

This library uses liquid SDR to encode and decode data through 44.1kHz. This makes it suitable for sending data across a 3.5mm headphone jack from one device to another. It can also be built using emscripten to generate a .js file which can do transmission and reception from the browser.

Building this project on a modern computer is somewhat tricky as libfec does not know about 64-bit architectures. I have a customized version which can handle this case which should be coming soon.

Generating the emscripten build requires that all dependencies be built with emscripten (e.g. emconfigure ./configure; emmake make; make install) for each dependency. I will eventually provide a built version of this js file for those who wish to just use it and go.

Profiles
-----------
The encoding and decoding process are controlled by the profiles in `web/profiles.json`. Each profile contains a complete set of parameters such as modem type and error correction. This project uses libjansson to read the profiles and then select one based on a given profile name.

Dependencies
-----------
[https://github.com/jgaeddert/liquid-dsp](Liquid DSP)
[http://www.ka9q.net/code/fec/](libfec)
[https://github.com/akheron/jansson](Jansson)
