var SampleEncoder = SampleEncoder || {};

var Module = {
    onFileSelect: function(e) {
        var payload_str = padding.join('') + payloadarray.join('');
        payload_str = payload_str.repeat(2);
        var payload = intArrayFromString(payload_str);
        ccall('encoder_set_payload', 'number', ['pointer', 'array', 'number'], [encoder, payload, payload.length]);

        var sample_len = 16384;
        var samples = ccall('malloc', 'pointer', ['number'], [4 * sample_len]);
        var sample_view = HEAPF32.subarray((samples/4), (samples/4) + sample_len);

        var script_processor = audio_ctx.createScriptProcessor || audio_ctx.createJavaScriptNode
        var transmitter = script_processor.call(audio_ctx, sample_len, 1, 2);
        transmitter.onaudioprocess = function(e) {
            var output_offset = 0;
            var output_l = e.outputBuffer.getChannelData(0);
            var written = ccall('encode', 'number', ['pointer', 'pointer', 'number'], [encoder, samples, sample_len]);
            output_l.set(sample_view);
            if (written < sample_len) {
                output_l.fill(0, written);
            }
        };


        setTimeout(function() {
            transmitter.connect(audio_ctx.destination);
        }, 5000);

    },
    onProfilesFetch: function(profiles) {
        var audio_ctx = new (window.AudioContext || window.webkitAudioContext)();
        console.log(audio_ctx.sampleRate);

        Module.memoryInitializerPrefixURL = "javascripts/";
        var c_profiles = intArrayFromString(profiles);
        var c_profilename = intArrayFromString("main");
        var opt = ccall('get_encoder_profile_str', 'pointer', ['array', 'array'], [c_profiles, c_profilename]);
        var encoder = ccall('create_encoder', 'pointer', ['pointer'], [opt]);

        document.querySelectorAll('[data-quiet-file-input]').addEventListener('change', Module.onFileSelect, false);


        //ccall('destroy_encoder', null, ['pointer'], [encoder]);

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
