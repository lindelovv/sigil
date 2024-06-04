package sigil
import "core:dynlib"
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

instance      : vk.Instance
device        : vk.Device
phys_device   : vk.PhysicalDevice
surface       : vk.SurfaceKHR
swapchain     : vk.SwapchainKHR
window_extent := vk.Extent2D { 1920, 1080 }

//_____________________________
load_vk_dll :: proc() {
    when ODIN_OS == .Linux {
        _vk_lib, loaded := dynlib.load_library("libvulkan.so.1", true); if !loaded {
            _vk_lib, loaded = dynlib.load_library("libvulkan.so", true)   
        }
        check_err(loaded, "Vulkan lib failed to load.")
    } else when ODIN_OS == .Darwin {
        _vk_lib, loaded := dynlib.load_library("libvulkan.dynlib", true); if !loaded {
            _vk_lib, loaded = dynlib.load_library("libvulkan.so", true); if !loaded {
                _vk_lib, loaded = dynlib.load_library("libvulkan.so", true)
            }
        }
    } else when ODIN_OS == .Windows {
        _vk_lib, loaded := dynlib.load_library("vulkan-1.dll", true)
    } else {
        panic("Non-supported OS.")
    }
    if !loaded || _vk_lib == nil {
        panic("Failed to load Vulkan library.")
    }

    get_proc_addr, found := dynlib.symbol_address(_vk_lib, "vkGetInstanceProcAddr")
    check_err(found, "Failed to find Vulkan lib symbol address.")

    vk.load_proc_addresses(get_proc_addr)
}

//_____________________________
init_vulkan :: proc() {

    load_vk_dll()

    create_info: vk.InstanceCreateInfo = {
        pApplicationInfo = &engine_info,
    }
    // fill out struct
    check_err(vk.CreateInstance(&create_info, nil, &instance), "VK: Instance Creation failed")
}

//_____________________________
tick_vulkan :: proc() {

}

