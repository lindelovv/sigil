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

Frame :: struct {
    pool: vk.CommandPool,
    cmd: vk.CommandBuffer,
    fence: vk.Fence,
    swap_sem: vk.Semaphore,
    render_sem: vk.Semaphore,
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
queue_family  : u32
queue         : vk.Queue
surface       : vk.SurfaceKHR
swapchain     : vk.SwapchainKHR
images        : [dynamic]vk.Image
img_views     : [dynamic]vk.ImageView
window_extent := vk.Extent2D { 1920, 1080 }
resize_window : bool
frames        : [2]Frame
current_frame : u32

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
    check_err(glfw.CreateWindowSurface(instance, window, nil, &surface), "Failed to create window surface")

    phys_count: u32
    check_err(vk.EnumeratePhysicalDevices(instance, &phys_count, nil), "VK: EnumeratePhysicalDevices Error")
    phys_device_list := make([dynamic]vk.PhysicalDevice, phys_count)
    check_err(vk.EnumeratePhysicalDevices(instance, &phys_count, raw_data(phys_device_list)), "VK: EnumeratePhysicalDevices Error")
    phys_device = phys_device_list[0]

    queue_priorities: f32 = 1.0
    queue_info := vk.DeviceQueueCreateInfo {
        sType            = .DEVICE_QUEUE_CREATE_INFO,
        queueFamilyIndex = queue_family,
        queueCount       = 1,
        pQueuePriorities = &queue_priorities
    }

    features12 := vk.PhysicalDeviceVulkan12Features {
        sType = .PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        bufferDeviceAddress = true,
        descriptorIndexing = true,
    }
    features := vk.PhysicalDeviceVulkan13Features {
        sType = .PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        pNext = &features12,
        synchronization2 = true,
        dynamicRendering = true,
    }

    device_create_info := vk.DeviceCreateInfo {
        sType                   = .DEVICE_CREATE_INFO,
        pNext                   = &features,
        queueCreateInfoCount    = 1,
        pQueueCreateInfos       = &queue_info,
        enabledLayerCount       = u32(len(validation_layers)),
        ppEnabledLayerNames     = raw_data(validation_layers),
        enabledExtensionCount   = u32(len(device_extensions)),
        ppEnabledExtensionNames = raw_data(device_extensions)
    }
    vk.CreateDevice(phys_device, &device_create_info, nil, &device)
    vk.GetDeviceQueue(device, queue_family, 0, &queue)

    width, height := glfw.GetFramebufferSize(window)
    swapchain_create_info := vk.SwapchainCreateInfoKHR {
        sType            = .SWAPCHAIN_CREATE_INFO_KHR,
        presentMode      = .FIFO,
        compositeAlpha   = { .OPAQUE },
        imageArrayLayers = 1,
        imageColorSpace  = .SRGB_NONLINEAR,
        surface          = surface,
        imageUsage       = { .COLOR_ATTACHMENT, .TRANSFER_DST },
        imageFormat      = .B8G8R8A8_UNORM,
        preTransform     = { .IDENTITY },
        imageExtent      = { u32(width), u32(height) },
        minImageCount    = 4, // TODO: properly check
    }
    check_err(vk.CreateSwapchainKHR(device, &swapchain_create_info, nil, &swapchain), "Failed to create swapchain")

    swapchain_img_count: u32
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, nil)
    images = make([dynamic]vk.Image, swapchain_img_count)
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, raw_data(images))

    for i: u32 = 0; i < swapchain_img_count; i += 1 {
        img_view: vk.ImageView
        img_view_create_info := vk.ImageViewCreateInfo {
            sType            = .IMAGE_VIEW_CREATE_INFO,
            viewType         = .D2,
            format           = .B8G8R8A8_UNORM,
            image            = images[i],
            subresourceRange = {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            }
        }
        vk.CreateImageView(device, &img_view_create_info, nil, &img_view)
        append(&img_views, img_view)
    }
    for &frame in frames {
        pool_create_info := vk.CommandPoolCreateInfo {
            sType = .COMMAND_POOL_CREATE_INFO,
            queueFamilyIndex = queue_family,
            flags = { .RESET_COMMAND_BUFFER },
        }
        check_err(vk.CreateCommandPool(device, &pool_create_info, nil, &frame.pool), "Failed to create command pool")

        cmd_create_info := vk.CommandBufferAllocateInfo {
            sType = .COMMAND_BUFFER_ALLOCATE_INFO,
            commandPool = frame.pool,
            level = .PRIMARY,
            commandBufferCount = 1
        }
        check_err(vk.AllocateCommandBuffers(device, &cmd_create_info, &frame.cmd), "Failed to create command buffer")

        fence_create_info := vk.FenceCreateInfo {
            sType = .FENCE_CREATE_INFO,
            flags = { .SIGNALED }
        }
        check_err(vk.CreateFence(device, &fence_create_info, nil, &frame.fence), "Failed to create fence")

        semaphore_create_info := vk.SemaphoreCreateInfo {
            sType = .SEMAPHORE_CREATE_INFO
        }
        check_err(vk.CreateSemaphore(device, &semaphore_create_info, nil, &frame.swap_sem), "Failed to create swap semaphore")
        check_err(vk.CreateSemaphore(device, &semaphore_create_info, nil, &frame.render_sem), "Failed to create render semaphore")
    }
}

//_____________________________
tick_vulkan :: proc() {
    //if resize_window { rebuild_swapchain() }
    frame := frames[current_frame]
    check_err(vk.WaitForFences(device, 1, &frame.fence, true, max(u64)), "Wait for fence failed")
    check_err(vk.ResetFences(device, 1, &frame.fence), "Reset fence failed")

    img_index: u32
    check_err(vk.AcquireNextImageKHR(device, swapchain, max(u64), frame.swap_sem, {}, &img_index), "Failed to acquire next swapchain image")
    img := images[img_index]
    vk.ResetCommandBuffer(frame.cmd, {})
    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }
    check_err(vk.BeginCommandBuffer(frame.cmd, &cmd_begin_info), "Begin cmd failed")
    {
        mem_barrier := vk.ImageMemoryBarrier2 {
            sType = .IMAGE_MEMORY_BARRIER_2,
            image = img,
            srcStageMask = { .ALL_COMMANDS },
            srcAccessMask = { .MEMORY_WRITE },
            dstStageMask = { .ALL_COMMANDS },
            dstAccessMask = { .MEMORY_WRITE, .MEMORY_READ },
            oldLayout = .UNDEFINED,
            newLayout = .GENERAL,
            srcQueueFamilyIndex = queue_family,
            subresourceRange = {
                aspectMask = { .COLOR },
                layerCount = 1,
                levelCount = 1
            }
        }
        dependency_info := vk.DependencyInfo {
            sType = .DEPENDENCY_INFO,
            imageMemoryBarrierCount = 1,
            pImageMemoryBarriers = &mem_barrier
        }
        vk.CmdPipelineBarrier2(frame.cmd, &dependency_info)

        clear_val := vk.ClearColorValue { float32 = { 0, 0, 0, 1 } }
        range_clear := vk.ImageSubresourceRange {
            aspectMask = { .COLOR },
            levelCount = vk.REMAINING_MIP_LEVELS,
            layerCount = vk.REMAINING_ARRAY_LAYERS
        }
        vk.CmdClearColorImage(frame.cmd, img, .GENERAL, &clear_val, 1, &range_clear)

        mem_barrier.oldLayout = .GENERAL
        mem_barrier.newLayout = .PRESENT_SRC_KHR

        vk.CmdPipelineBarrier2(frame.cmd, &dependency_info)
    }
    check_err(vk.EndCommandBuffer(frame.cmd), "End cmd failed")

    stage_mask := vk.PipelineStageFlags { .ALL_COMMANDS }
    submit_info := vk.SubmitInfo {
        sType = .SUBMIT_INFO,
        commandBufferCount = 1,
        pCommandBuffers = &frame.cmd,
        waitSemaphoreCount = 1,
        pWaitSemaphores = &frame.swap_sem,
        signalSemaphoreCount = 1,
        pSignalSemaphores = &frame.render_sem,
        pWaitDstStageMask = &stage_mask
    }
    check_err(vk.QueueSubmit(queue, 1, &submit_info, frame.fence), "Queue submit failed")

    present_info := vk.PresentInfoKHR {
        sType = .PRESENT_INFO_KHR,
        waitSemaphoreCount = 1,
        pWaitSemaphores = &frame.render_sem,
        pImageIndices = &img_index,
        swapchainCount = 1,
        pSwapchains = &swapchain
    }
    check_err(vk.QueuePresentKHR(queue, &present_info), "Queue present failed")

    current_frame = current_frame > 0 ? 0 : current_frame + 1
}

