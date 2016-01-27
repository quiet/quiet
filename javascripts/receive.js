var SampleDecoder = SampleDecoder || {};

var Module = {
    onProfilesFetch: function(profiles) {
        var c_profiles = intArrayFromString(profiles);
        var c_profilename = intArrayFromString("main");
        var opt = ccall('get_decoder_profile_str', 'pointer', ['array', 'array'], [c_profiles, c_profilename]);
        var decoder = ccall('create_decoder', 'pointer', ['pointer'], [opt]);
        var sample_buffer_size = 16384;
        var sample_buffer = ccall('malloc', 'pointer', ['number'], [4 * sample_buffer_size]);
        var data_buffer_size = Math.pow(2, 16);
        var data_buffer = ccall('malloc', 'pointer', ['number'], [data_buffer_size]);
        var getUserMedia = (navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia);
        getUserMedia.call(navigator, {
                audio: {
                    optional: [
                      {googAutoGainControl: false},
                      {googAutoGainControl2: false},
                      {googEchoCancellation: false},
                      {googEchoCancellation2: false},
                      {googNoiseSuppression: false},
                      {googNoiseSuppression2: false},
                      {googHighpassFilter: false},
                      {googTypingNoiseDetection: false},
                      {googAudioMirroring: false}
                    ]
                }
            }, function(e){
            var context = new (window.AudioContext || window.webkitAudioContext)();
            var audioInput = context.createMediaStreamSource(e);
            window.stupid_anti_gc = audioInput;
            window.recorder = context.createScriptProcessor(16384, 2, 1);

            window.recorder.onaudioprocess = function(e) {
                var input = e.inputBuffer.getChannelData(0);
                var sample_view = HEAPF32.subarray(sample_buffer/4, sample_buffer/4 + sample_buffer_size);
                sample_view.set(input);
                var data_buffered = ccall('decode', 'number', ['pointer', 'pointer', 'number'], [decoder, sample_buffer, sample_buffer_size]);

                if (data_buffered > data_buffer_size) {
                    data_buffer = ccall('realloc', 'pointer', ['pointer', 'number'], [data_buffer, data_buffered]);
                    data_buffer_size = data_buffered;
                }

                if (data_buffered > 0) {
                    ccall('decoder_readbuf', 'number', ['pointer', 'pointer', 'number'], [decoder, data_buffer, data_buffered]);
                    var result = HEAP8.subarray(data_buffer, data_buffer + data_buffered)
                    var result_str = String.fromCharCode.apply(null, new Uint8Array(result));
                    var div = document.getElementById('payload');
                    div.innerHTML = div.innerHTML + result_str;
                }
            }

            audioInput.connect(window.recorder);

            var fakeGain = context.createGain();
            fakeGain.value = 0;
            window.recorder.connect(fakeGain);
            fakeGain.connect(context.destination);


        }, function(){});
        // ccall('destroy_decoder', null, ['pointer'], [decoder]);
    },
    onRuntimeInitialized: function() {
        var xhr = new XMLHttpRequest();
        xhr.overrideMimeType("application/json");
        xhr.open("GET", "profiles.json", true);
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4 && xhr.status == "200") {
                Module.onProfilesFetch(xhr.responseText);
            }
        };
        xhr.send(null);
    }
};
