//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
package sigil
import "vendor:wgpu"

WGPU :: struct {
    surface : wgpu.Surface,
}; wgpu_data :: WGPU
wgpu :: Module {
    setup = proc() {
        schedule(.INIT, init_wgpu)
        //schedule(.TICK, tick_vulkan)
        //schedule(.EXIT, terminate_vulkan)
    },
    data = wgpu_data,
}

init_wgpu :: proc() {

}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
