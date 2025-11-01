package ism

// todo: fix up wasm compilation before working on this

//import "base:runtime"
//import "vendor:wgpu"
//import "core:sys/wasm/js"
//import "core:fmt"
//import sigil "sigil:core"
//
//wgpu := sigil.module_create_info_t {
//    name     = "webgpu_module",
//    schedule = []sigil.schedule_function {
//        { sigil.init, init_wgpu },
//        { sigil.tick, tick_wgpu },
//        { sigil.exit, exit_wgpu },
//    },
//}
//
//renderer: struct {
//    instance : wgpu.Instance,
//    surface  : wgpu.Surface,
//    adapter  : wgpu.Adapter,
//    device   : wgpu.Device,
//    config   : wgpu.SurfaceConfiguration,
//    queue    : wgpu.Queue,
//    module   : wgpu.ShaderModule,
//    pipeline : wgpu.RenderPipeline,
//    layout   : wgpu.PipelineLayout,
//}
//
//init_wgpu :: proc() {
//    js.add_window_event_listener(.Resize, nil, size_callback)
//    renderer.instance = wgpu.CreateInstance(nil)
//    __ensure(renderer.instance, "WebGPU is not supported")
//    renderer.surface = wgpu.InstanceCreateSurface(
//        renderer.instance, 
//        &wgpu.SurfaceDescriptor { 
//            nextInChain = &wgpu.SurfaceSourceCanvasHTMLSelector {
//                sType = .SurfaceSourceCanvasHTMLSelector,
//                selector = "#wgpu-canvas",
//            },
//        },
//    )
//    wgpu.InstanceRequestAdapter(renderer.instance, &{ compatibleSurface = renderer.surface }, { callback = on_adapter })
//    on_adapter :: proc "c" (status: wgpu.RequestAdapterStatus, adapter: wgpu.Adapter, msg: string, _, _: rawptr) {
//        context = runtime.default_context()
//        __ensure_bool((status == .Success || adapter != nil), fmt.aprintf("Failed requesting adapter: [%v] %s", status, msg, allocator = context.temp_allocator))
//        renderer.adapter = adapter
//        wgpu.AdapterRequestDevice(adapter, nil, { callback = on_device })
//    }
//    on_device :: proc "c" (status: wgpu.RequestDeviceStatus, device: wgpu.Device, msg: string, _, _: rawptr) {
//        context = runtime.default_context()
//        __ensure_bool((status == .Success || device != nil), fmt.aprintf("Failed requesting device: [%v] %s", status, msg, allocator = context.temp_allocator))
//        renderer.device = device
//        width, height := get_framebuffer_size()
//    }
//    get_framebuffer_size :: proc() -> (u32, u32) {
//        rect := js.get_bounding_client_rect("body")
//        dpi := f64(1)//js.device_pixel_ratio() // should exist but does not, am I just stupid ?
//        return u32(f64(rect.width) * dpi), u32(f64(rect.height) * dpi)
//    }
//    size_callback :: proc(e: js.Event) {
//        renderer.config.width, renderer.config.height = get_framebuffer_size()
//        wgpu.SurfaceConfigure(renderer.surface, &renderer.config)
//    }
//}
//
//tick_wgpu :: proc() {
//
//}
//
//exit_wgpu :: proc() {
//
//}

