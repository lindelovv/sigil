package ism

import sigil "sigil:core"
import ma "vendor:miniaudio"
import "vendor:glfw"

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

miniaudio := sigil.module_create_info_t {
    name  = "miniaudio_module",
    setup = proc(world: ^sigil.world_t, e: sigil.entity_t) {
        sigil.add_component(world, e, sigil.init(init_miniaudio))
        sigil.add_component(world, e, sigil.exit(deinit_miniaudio))
    },
}

engine: ma.engine
audio_device: ma.device
//audio_time: f32 = 0.0

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

//sine_callback :: proc "c" (device: ^ma.device, output: rawptr, input: rawptr, frame_count: u32) {
//    samples := ([^]f32)(output)
//    u_ptr := (cast(^f32)device.pUserData)
//    time := u_ptr^
//    freq := 440.0
//    sample_rate := device.sampleRate
//    for i in 0..< frame_count {
//        samples[i] = math.sin(f32(2.0 * math.PI * freq) * time)
//        time += f32(1 / sample_rate)
//    }
//    u_ptr^ = time
//}

init_miniaudio :: proc(world: ^sigil.world_t) {
    //conf := ma.device_config_init(.playback)
    //conf.playback.format    = ma.format.f32
    //conf.playback.channels  = 1
    //conf.sampleRate         = 44100
    //conf.dataCallback       = sine_callback
    //conf.pUserData          = &audio_time
    //__ensure(ma.device_init(nil, &conf, &audio_device), "failed to initialize playback device")
    //ma.device_start(&audio_device)

    __ensure(ma.engine_init(nil, &engine), "failed to initialize miniaudio")
    bind_input(glfw.KEY_P,
        press   = proc(^sigil.world_t) {
            __ensure(ma.engine_play_sound(&engine, "res/test.mp3", nil), "failed play sound")
        },
    )
}

deinit_miniaudio :: proc(world: ^sigil.world_t) {
    //ma.device_uninit(&audio_device)
    ma.engine_uninit(&engine)
}

