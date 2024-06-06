package sigil
import "core:dynlib"
import "base:runtime"
import vk "vendor:vulkan"
import "vendor:glfw"

__vulkan :: Module {
    setup = proc() {
        using eRUNLVL
        schedule(init, init_vulkan)
        schedule(tick, tick_vulkan)
    }
}

//_____________________________
engine_info := vk.ApplicationInfo {
    pApplicationName    = "sigil",
    applicationVersion  = SIGIL_V,
    pEngineName         = "sigil",
    engineVersion       = SIGIL_V,
}

validation_layers := [dynamic]cstring { "VK_LAYER_KHRONOS_validation" }
device_extensions := [dynamic]cstring {
    vk.KHR_SWAPCHAIN_EXTENSION_NAME,
    vk.KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    vk.KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
}

instance      : vk.Instance
device        : vk.Device
phys_device   : vk.PhysicalDevice
surface       : vk.SurfaceKHR
swapchain     : vk.SwapchainKHR
window_extent := vk.Extent2D { 1920, 1080 }
resize_window : bool

//_____________________________
dbg_messenger : vk.DebugUtilsMessengerEXT
dbg_callback  : vk.ProcDebugUtilsMessengerCallbackEXT : proc "stdcall"(
    msg_severity:       vk.DebugUtilsMessageSeverityFlagsEXT,
    msg_types:          vk.DebugUtilsMessageTypeFlagsEXT,
    p_callback_data:    ^vk.DebugUtilsMessengerCallbackDataEXT, 
    p_user_data:        rawptr
) -> b32 {
    context = runtime.default_context()
    when ODIN_DEBUG do dbg_msg(msg_severity, p_callback_data.pMessage)
    return false
}

//_____________________________
init_vulkan :: proc() {
    vk.load_proc_addresses_global(rawptr(glfw.GetInstanceProcAddress))

    append(&device_extensions, ..glfw.GetRequiredInstanceExtensions())

    when ODIN_DEBUG {
        append(&device_extensions, vk.EXT_DEBUG_UTILS_EXTENSION_NAME)
        dbg_msg("Device Extension Count:", u32(len(device_extensions)))

        create_info := vk.InstanceCreateInfo {
            sType                   = .INSTANCE_CREATE_INFO,
            pApplicationInfo        = &engine_info,
            // EXT
            enabledExtensionCount   = u32(len(device_extensions)),
            ppEnabledExtensionNames = raw_data(device_extensions),
            // LAYERS
            enabledLayerCount       = u32(len(validation_layers)),
            ppEnabledLayerNames     = raw_data(validation_layers),
        }
    } else {
        create_info := vk.InstanceCreateInfo {
            sType                   = .INSTANCE_CREATE_INFO,
            pApplicationInfo        = &engine_info,
            // EXT
            enabledExtensionCount   = u32(len(device_extensions)),
            ppEnabledExtensionNames = raw_data(device_extensions),
            // LAYERS
            enabledLayerCount       = 0,
        }
    }
    check_err(vk.CreateInstance(&create_info, nil, &instance), "VK: Instance Creation failed")

    get_instance_proc_addr :: proc "system"(instance: vk.Instance, name: cstring) -> vk.ProcVoidFunction {
        return (vk.ProcVoidFunction)(glfw.GetInstanceProcAddress(instance, name))
    }
    vk.GetInstanceProcAddr = get_instance_proc_addr
    vk.load_proc_addresses(instance)

    when ODIN_DEBUG {
        dbg_create_info: vk.DebugUtilsMessengerCreateInfoEXT = {
            messageSeverity = { .VERBOSE, .WARNING, .ERROR, .INFO },
            messageType     = { .GENERAL, .VALIDATION, .PERFORMANCE },
            pfnUserCallback = dbg_callback,
        }
        check_err(vk.CreateDebugUtilsMessengerEXT(instance, &dbg_create_info, nil, &dbg_messenger), "VK: Debug Utils Messenger Creation Failed")
    }
}

//_____________________________
tick_vulkan :: proc() {

}
