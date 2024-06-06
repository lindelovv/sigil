package sigil
import "core:dynlib"
import "core:slice"
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
    sType               = .APPLICATION_INFO,
    pApplicationName    = "sigil",
    applicationVersion  = SIGIL_V,
    pEngineName         = "sigil",
    engineVersion       = SIGIL_V,
    apiVersion          = vk.API_VERSION_1_3,
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
dbg_callback  : vk.ProcDebugUtilsMessengerCallbackEXT : proc "cdecl" (
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

    extensions := slice.clone_to_dynamic(glfw.GetRequiredInstanceExtensions(), context.temp_allocator)

    when ODIN_DEBUG {
        append(&extensions, vk.EXT_DEBUG_UTILS_EXTENSION_NAME)

        //layer_count: u32
        //vk.EnumerateInstanceLayerProperties(&layer_count, nil)
        //available_layers := make([dynamic]vk.LayerProperties, layer_count)
        //defer delete(available_layers)
        //vk.EnumerateInstanceLayerProperties(&layer_count, raw_data(available_layers))
        //lay_out: for layer in validation_layers {
        //    for avail in available_layers {
        //        bytes := avail.layerName
        //        name_slice := bytes[:]
        //        if cstring(raw_data(name_slice)) == layer do continue lay_out
        //    }
        //    dbg_msg("Layer Unavailable:", layer)
        //    panic("")
        //}

        //extension_count: u32
        //vk.EnumerateInstanceExtensionProperties(nil, &extension_count, nil)
        //available_extensions := make([dynamic]vk.ExtensionProperties, extension_count)
        //defer delete(available_extensions)
        //vk.EnumerateInstanceExtensionProperties(nil, &extension_count, raw_data(available_extensions))
        //ex_out: for extension in extensions {
        //    for avail in available_extensions {
        //        bytes := avail.extensionName
        //        name_slice := bytes[:]
        //        if cstring(raw_data(name_slice)) == extension do continue ex_out
        //    }
        //    dbg_msg("Extension Unavailable:", extension)
        //    panic("")
        //}

        create_info := vk.InstanceCreateInfo {
            sType                   = .INSTANCE_CREATE_INFO,
            pApplicationInfo        = &engine_info,
            // EXT
            enabledExtensionCount   = u32(len(extensions)),
            ppEnabledExtensionNames = raw_data(extensions),
            // LAYERS
            enabledLayerCount       = u32(len(validation_layers)),
            ppEnabledLayerNames     = raw_data(validation_layers),
        }
    } else {
        create_info := vk.InstanceCreateInfo {
            sType                   = .INSTANCE_CREATE_INFO,
            pApplicationInfo        = &engine_info,
            // EXT
            enabledExtensionCount   = u32(len(extensions)),
            ppEnabledExtensionNames = raw_data(extensions),
            // LAYERS
            enabledLayerCount       = 0,
            ppEnabledLayerNames     = nil,
        }
    }
    check_err(vk.CreateInstance(&create_info, nil, &instance), "VK: Instance Creation failed")
    vk.load_proc_addresses(instance)

    when ODIN_DEBUG {
        dbg_create_info: vk.DebugUtilsMessengerCreateInfoEXT = {
            sType           = .DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            messageSeverity = { .VERBOSE, .WARNING, .ERROR, .INFO },
            messageType     = { .GENERAL, .VALIDATION, .PERFORMANCE },
            pfnUserCallback = dbg_callback,
        }
        check_err(vk.CreateDebugUtilsMessengerEXT(instance, &dbg_create_info, nil, &dbg_messenger), "VK: Debug Utils Messenger Creation Failed")
    }
    glfw.CreateWindowSurface(instance, window, nil, &surface)

    //phys_count: u32
    //vk.EnumeratePhysicalDevices(instance, &phys_count, &phys_device)
}

//_____________________________
tick_vulkan :: proc() {

}

