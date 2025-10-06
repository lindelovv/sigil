package ism

import sigil "sigil:core"
import "core:dynlib"
import "core:slice"
import "core:strings"
import "base:runtime"
import vk "vendor:vulkan"
import vma "lib:odin-vma"
import "vendor:glfw"
import "core:os"
import "core:fmt"
import "core:mem"
import "core:math"
import "core:math/linalg"
import glm "core:math/linalg/glsl"
import "vendor:stb/image"
import "vendor:cgltf"
import "lib:slang"
import "core:encoding/json"
import "lib:mikktspace"

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
vulkan := sigil.module_create_info_t {
    name  = "vulkan_module",
    setup = proc(e: sigil.entity_t) {
        using sigil
        add(e, init(init_vulkan))
        add(e, tick(tick_vulkan))
        add(e, exit(terminate_vulkan))
    },
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
engine_info := vk.ApplicationInfo {
    sType               = .APPLICATION_INFO,
    pApplicationName    = "sigil",
    applicationVersion  = VK_VERSION,
    pEngineName         = "sigil",
    engineVersion       = VK_VERSION,
    apiVersion          = vk.API_VERSION_1_3,
}
VK_VERSION := vk.MAKE_VERSION(sigil.MAJOR_V, sigil.MINOR_V, sigil.PATCH_V)

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
validation_layers := []cstring {
    "VK_LAYER_KHRONOS_validation",
}

device_extensions := []cstring {
    vk.KHR_SWAPCHAIN_EXTENSION_NAME,
    vk.KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    vk.KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
instance         : vk.Instance
device           : device_t
vma_allocator    : vma.Allocator
queue            : queue_t
surface          : vk.SurfaceKHR
swapchain        : swapchain_t
resize_window    : bool = true

device_t :: struct {
    handle     : vk.Device,
    physical   : vk.PhysicalDevice,
    properties : vk.PhysicalDeviceProperties,
}

queue_t :: struct {
    handle : vk.Queue,
    family : u32,
}

swapchain_t :: struct {
    handle : vk.SwapchainKHR,
    images : [dynamic]vk.Image,
    views  : [dynamic]vk.ImageView,
    extent : vk.Extent2D,
}

frames           : [2]frame_t
current_frame    : u32

desc_pool        : vk.DescriptorPool
draw_img         : allocated_image_t
draw_descriptor  : vk.DescriptorSet
draw_extent      : vk.Extent2D
depth_img        : allocated_image_t
immediate        : immediate_submit_t
sampler          : vk.Sampler

MAX_OBJECTS :: 100_000

transform_buffer : allocated_buffer_t
scene_ubo        : allocated_buffer_t
scene_descriptor : descriptor_data_t

bindless         : material_t
textures         : [dynamic]vk.ImageView
texture_indices  : map[vk.ImageView]u32
buffers          : [dynamic]vk.Buffer
buffer_indices   : map[vk.Buffer]u32

materials        : [4096]material_t
material_count   : u32

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
global_session   : ^slang.IGlobalSession

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
error_image      : allocated_image_t
scene_data       : scene_data_t
gpu_scene_data   : gpu_scene_data_t
scene_allocation : allocated_buffer_t

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
frame_t :: struct {
    pool       : vk.CommandPool,
    cmd        : vk.CommandBuffer,
    fence      : vk.Fence,
    swap_sem   : vk.Semaphore,
    render_sem : vk.Semaphore,
    descriptor : descriptor_data_t,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
immediate_submit_t :: struct {
    fence: vk.Fence,
    cmd  : vk.CommandBuffer,
    pool : vk.CommandPool,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
allocated_buffer_t :: struct {
    handle     : vk.Buffer,
    index      : u32,
    allocation : vma.Allocation,
    info       : vma.AllocationInfo,
}

allocated_image_t :: struct {
    handle     : vk.Image,
    index      : u32,
    view       : vk.ImageView,
    allocation : vma.Allocation,
    extent     : vk.Extent3D,
    format     : vk.Format,
}

allocated_handle_t :: union {
    allocated_buffer_t, 
    allocated_image_t,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
vertex_t :: struct {
    position : [3]f32,
    uv_x     : f32,
    normal   : [3]f32,
    uv_y     : f32,
    tangent  : [4]f32,
    color    : [4]f32,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
render_data_t :: struct {
    count           : u32,
    first           : u32,
    idx_buffer      : vk.Buffer,
    //material        : ^material_t,
    address         : vk.DeviceAddress,
    indirect_offset : vk.DeviceSize,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
material_t :: struct {
    sampler         : vk.Sampler,
    data            : vk.Buffer,
    offset          : u32,
    set_layout      : vk.DescriptorSetLayout,
    set             : vk.DescriptorSet,
    pool            : vk.DescriptorPool,
    pipeline        : vk.Pipeline,
    pipeline_layout : vk.PipelineLayout,
    update_delegate : proc(vk.CommandBuffer, ^render_data_t),
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
descriptor_data_t :: struct {
    set         : vk.DescriptorSet,
    bindings    : [dynamic]vk.DescriptorSetLayoutBinding,
    buffer_info : [dynamic]vk.DescriptorBufferInfo,
    image_info  : [dynamic]vk.DescriptorImageInfo,
}

descriptor_info_t :: union {
    descriptor_image_info_t,
    descriptor_buffer_info_t
}
descriptor_image_info_t :: struct {
    view    : vk.ImageView,
    sampler : vk.Sampler,
    layout  : vk.ImageLayout,
}
descriptor_buffer_info_t :: struct {
    buffer  : allocated_buffer_t,
    size    : vk.DeviceSize,
    offset  : vk.DeviceSize,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
scene_data_t :: struct {
    set_layout      : vk.DescriptorSetLayout,
    set             : vk.DescriptorSet,
    pool            : vk.DescriptorPool,
    pipeline        : vk.Pipeline,
    pipeline_layout : vk.PipelineLayout,
}
gpu_scene_data_t :: struct #align(16) {
    view          : glm.mat4,
    proj          : glm.mat4,
    ambient_color : glm.vec4,
    sun_color     : glm.vec4,
    sun_direction : glm.vec3,
    time          : f32,
    view_pos      : glm.vec3,
    frame_index   : u32,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
push_constant_t :: struct #align(16) {
    model         : u32,
    vertex_buffer : vk.DeviceAddress,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
transform_t :: distinct glm.mat4
position_t :: distinct [3]f32
rotation_t :: distinct quaternion128
velocity_t :: glm.vec3

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
dbg_messenger : vk.DebugUtilsMessengerEXT
dbg_callback /* +-+-+-+-+-+ */ :: proc "system" (
    msg_severity  : vk.DebugUtilsMessageSeverityFlagsEXT,
    msg_types     : vk.DebugUtilsMessageTypeFlagsEXT,
    callback_data : ^vk.DebugUtilsMessengerCallbackDataEXT, 
    user_data     : rawptr
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> b32 {
    context = runtime.default_context()
    when ODIN_DEBUG {
        if msg_severity == { .INFO    } do fmt.printfln("VK INFO:    \n%s\n", callback_data.pMessage)
        if msg_severity == { .VERBOSE } do fmt.printfln("VK VERBOSE: \n%s\n", callback_data.pMessage)
        if msg_severity == { .WARNING } do fmt.printfln("VK WARNING: \n%s\n", callback_data.pMessage)
        if msg_severity == { .ERROR   } do fmt.printfln("VK ERROR:   \n%s\n", callback_data.pMessage)
    }
    return false
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
init_vulkan :: proc() {
    vk.load_proc_addresses_global(rawptr(glfw.GetInstanceProcAddress))
    extensions := slice.clone_to_dynamic(glfw.GetRequiredInstanceExtensions(), context.temp_allocator)

    //_____________________________
    // Ensure required extensions are available
    extension_count: u32
    vk.EnumerateInstanceExtensionProperties(nil, &extension_count, nil)
    available_extensions := make([dynamic]vk.ExtensionProperties, extension_count)
    defer delete(available_extensions)

    vk.EnumerateInstanceExtensionProperties(nil, &extension_count, raw_data(available_extensions))
    ex_out: for extension in extensions {
        for avail in available_extensions {
            bytes := avail.extensionName
            name_slice := bytes[:]
            if cstring(raw_data(name_slice)) == extension do continue ex_out
        }
        panic(fmt.aprintf("Extension Unavailable: %s", extension))
    }

    when ODIN_DEBUG {
        append(&extensions, vk.EXT_DEBUG_UTILS_EXTENSION_NAME)

        //_____________________________
        // Ensure required layers are available
        layer_count: u32
        vk.EnumerateInstanceLayerProperties(&layer_count, nil)
        available_layers := make([dynamic]vk.LayerProperties, layer_count)
        defer delete(available_layers)

        vk.EnumerateInstanceLayerProperties(&layer_count, raw_data(available_layers))
        lay_out: for layer in validation_layers {
            for avail in available_layers {
                bytes := avail.layerName
                name_slice := bytes[:]
                if cstring(raw_data(name_slice)) == layer do continue lay_out
            }
            panic(fmt.aprintf("Layer Unavaileble: %s", layer))
        }

        //_____________________________
        // Instance
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
    __ensure(
        vk.CreateInstance(&create_info, nil, &instance), 
        msg = "VK: Instance Creation failed"
    )
    vk.load_proc_addresses(instance)

    //_____________________________
    // Debug Messenger
    when ODIN_DEBUG {
        dbg_create_info: vk.DebugUtilsMessengerCreateInfoEXT = {
            sType           = .DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            messageSeverity = { /*.WARNING, .ERROR , .VERBOSE, .INFO */ },
            messageType     = { .GENERAL, .VALIDATION, .PERFORMANCE },
            pfnUserCallback = dbg_callback
        }
        __ensure(
            vk.CreateDebugUtilsMessengerEXT(instance, &dbg_create_info, nil, &dbg_messenger), 
            msg = "VK: Debug Utils Messenger Creation Failed"
        )
    }

    //_____________________________
    // Surface
    __ensure(
        glfw.CreateWindowSurface(instance, window, nil, &surface), 
        msg = "Failed to create window surface"
    )

    //_____________________________
    // Physical Device
    phys_count: u32
    __ensure(
        vk.EnumeratePhysicalDevices(instance, &phys_count, nil), 
        msg = "VK: EnumeratePhysicalDevices Error"
    )
    phys_device_list := make([dynamic]vk.PhysicalDevice, phys_count)
    __ensure(
        vk.EnumeratePhysicalDevices(instance, &phys_count, raw_data(phys_device_list)), 
        msg = "VK: EnumeratePhysicalDevices Error"
    )
    // TODO: PROPERLY CHECK THIS, WOULD HAVE SAVED SO MUCH TIME ON LAPTOP DEBUGGING
    //       VERY DIRTY SOLUTION
    when ODIN_OS == .Linux {
        device.physical = phys_device_list[0]
    } else when ODIN_OS == .Windows {
        device.physical = phys_device_list[1]
    }

    //_____________________________
    // Device
    queue_priorities: f32 = 1.0
    queue_info := vk.DeviceQueueCreateInfo {
        sType            = .DEVICE_QUEUE_CREATE_INFO,
        queueFamilyIndex = queue.family,
        queueCount       = 1,
        pQueuePriorities = &queue_priorities,
    }
    features_vk_1_2 := vk.PhysicalDeviceVulkan12Features {
        sType     = .PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        bufferDeviceAddress                       = true,
        descriptorIndexing                        = true,
        runtimeDescriptorArray                    = true,
        descriptorBindingPartiallyBound           = true,
        descriptorBindingVariableDescriptorCount  = true,
        shaderSampledImageArrayNonUniformIndexing = true,
    }
    features_vk_1_3 := vk.PhysicalDeviceVulkan13Features {
        sType            = .PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        pNext            = &features_vk_1_2,
        synchronization2 = true,
        dynamicRendering = true,
    }
    device_create_info := vk.DeviceCreateInfo {
        sType                   = .DEVICE_CREATE_INFO,
        pNext                   = &features_vk_1_3,
        queueCreateInfoCount    = 1,
        pQueueCreateInfos       = &queue_info,
        enabledLayerCount       = u32(len(validation_layers)),
        ppEnabledLayerNames     = raw_data(validation_layers),
        enabledExtensionCount   = u32(len(device_extensions)),
        ppEnabledExtensionNames = raw_data(device_extensions),
    }
    vk.CreateDevice(device.physical, &device_create_info, nil, &device.handle)
    vk.GetDeviceQueue(device.handle, queue.family, 0, &queue.handle)

    //_____________________________
    // Allocator [VMA]
    vk_fns := vma.create_vulkan_functions()
    vma_info := vma.AllocatorCreateInfo {
        vulkanApiVersion = vk.API_VERSION_1_3,
        physicalDevice   = device.physical,
        device           = device.handle,
        instance         = instance,
        pVulkanFunctions = &vk_fns,
        flags            = { .BUFFER_DEVICE_ADDRESS },
    }
    __ensure(
        vma.CreateAllocator(&vma_info, &vma_allocator),
        msg = "Failed to create memory allocator"
    )

    //_____________________________
    // Swapchain
    capabilities: vk.SurfaceCapabilitiesKHR
    vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical, surface, &capabilities)
    width, height := glfw.GetFramebufferSize(window)
    swapchain.extent = vk.Extent2D { u32(width), u32(height) }
    swapchain_create_info := vk.SwapchainCreateInfoKHR {
        sType            = .SWAPCHAIN_CREATE_INFO_KHR,
        presentMode      = .IMMEDIATE,
        compositeAlpha   = { .OPAQUE },
        imageArrayLayers = 1,
        imageColorSpace  = .SRGB_NONLINEAR,
        surface          = surface,
        imageUsage       = { .TRANSFER_DST, .COLOR_ATTACHMENT, .STORAGE },
        imageFormat      = .B8G8R8A8_UNORM,
        preTransform     = { .IDENTITY },
        imageExtent      = swapchain.extent,
        minImageCount    = capabilities.minImageCount + 1,
    }
    __ensure(
        vk.CreateSwapchainKHR(device.handle, &swapchain_create_info, nil, &swapchain.handle),
        msg = "Failed to create swapchain"
    )

    //_____________________________
    // Images
    swapchain_img_count: u32
    vk.GetSwapchainImagesKHR(device.handle, swapchain.handle, &swapchain_img_count, nil)
    swapchain.images = make([dynamic]vk.Image, swapchain_img_count)
    vk.GetSwapchainImagesKHR(device.handle, swapchain.handle, &swapchain_img_count, raw_data(swapchain.images))

    for i: u32 = 0; i < swapchain_img_count; i += 1 {
        img_view: vk.ImageView
        img_view_create_info := vk.ImageViewCreateInfo {
            sType            = .IMAGE_VIEW_CREATE_INFO,
            viewType         = .D2,
            format           = .B8G8R8A8_UNORM,
            image            = swapchain.images[i],
            subresourceRange = {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            }
        }
        vk.CreateImageView(device.handle, &img_view_create_info, nil, &img_view)
        append(&swapchain.views, img_view)
    }

    //_____________________________
    // Bindless pool
    pool_sizes := []vk.DescriptorPoolSize {
        { type = .STORAGE_BUFFER, descriptorCount = 1 },
        { type = .SAMPLED_IMAGE,  descriptorCount = MAX_OBJECTS },
        { type = .SAMPLER,        descriptorCount = 100, },
    }
    bindless_desc_pool_info := vk.DescriptorPoolCreateInfo {
        sType         = .DESCRIPTOR_POOL_CREATE_INFO,
        flags         = { .UPDATE_AFTER_BIND },
        maxSets       = 10,
        poolSizeCount = u32(len(pool_sizes)),
        pPoolSizes    = raw_data(pool_sizes),
    }
    __ensure(
        vk.CreateDescriptorPool(device.handle, &bindless_desc_pool_info, nil, &bindless.pool),
        msg = "Failed to create bindless descriptor pool"
    )

    //_____________________________
    // Bindless Layout
    bindless_bindings := []vk.DescriptorSetLayoutBinding {
        {
            binding         = 0,
            descriptorType  = .STORAGE_BUFFER,
            descriptorCount = 1,
            stageFlags      = vk.ShaderStageFlags_ALL,
        },
        {
            binding         = 1,
            descriptorType  = .SAMPLED_IMAGE,
            descriptorCount = MAX_OBJECTS,
            stageFlags      = vk.ShaderStageFlags_ALL,
        },
        {
            binding         = 2,
            descriptorType  = .SAMPLER,
            descriptorCount = 1,
            stageFlags      = vk.ShaderStageFlags_ALL,
        },
    }
    bindless_set_layout_info := vk.DescriptorSetLayoutCreateInfo {
        sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        flags        = { .UPDATE_AFTER_BIND_POOL },
        bindingCount = u32(len(bindless_bindings)),
        pBindings    = raw_data(bindless_bindings),
    }
    __ensure(
        vk.CreateDescriptorSetLayout(device.handle, &bindless_set_layout_info, nil, &bindless.set_layout),
        msg = "Faield to create bindless descriptor set layout"
    )

    //_____________________________
    // Allocate Bindless Descriptor Set
    bindless_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = bindless.pool,
        descriptorSetCount = 1,
        pSetLayouts        = &bindless.set_layout,
    }
    __ensure(
        vk.AllocateDescriptorSets(device.handle, &bindless_allocate_info, &bindless.set),
        msg = "Failed to allocate bindless descriptor sets"
    )

    transform_size : vk.DeviceSize = size_of(glm.mat4) * MAX_OBJECTS
    transform_buffer = create_buffer(transform_size, { .STORAGE_BUFFER, .TRANSFER_DST }, .CPU_TO_GPU)
    transform_buffer.index = register_buffer(transform_buffer.handle, transform_size)

    //_____________________________
    // Draw & depth image
    draw_extent = swapchain.extent
    extent := vk.Extent3D { swapchain.extent.width, swapchain.extent.height, 1 }
    draw_img = create_image(.B8G8R8A8_UNORM, { .TRANSFER_SRC, .TRANSFER_DST, .STORAGE, .COLOR_ATTACHMENT }, extent)

    depth_img = create_image(.D32_SFLOAT, { .DEPTH_STENCIL_ATTACHMENT }, extent)

    //_____________________________
    // Frames
    for &frame in frames {
        pool_create_info := vk.CommandPoolCreateInfo {
            sType            = .COMMAND_POOL_CREATE_INFO,
            queueFamilyIndex = queue.family,
            flags            = { .RESET_COMMAND_BUFFER },
        }
        __ensure(
            vk.CreateCommandPool(device.handle, &pool_create_info, nil, &frame.pool),
            msg = "Failed to create command pool"
        )

        cmd_create_info := vk.CommandBufferAllocateInfo {
            sType              = .COMMAND_BUFFER_ALLOCATE_INFO,
            commandPool        = frame.pool,
            level              = .PRIMARY,
            commandBufferCount = 1
        }
        __ensure(
            vk.AllocateCommandBuffers(device.handle, &cmd_create_info, &frame.cmd),
            msg = "Failed to create command buffer"
        )

        fence_create_info := vk.FenceCreateInfo {
            sType = .FENCE_CREATE_INFO,
            flags = { .SIGNALED }
        }
        __ensure(
            vk.CreateFence(device.handle, &fence_create_info, nil, &frame.fence),
            msg = "Failed to create fence"
        )

        semaphore_create_info := vk.SemaphoreCreateInfo {
            sType = .SEMAPHORE_CREATE_INFO
        }
        __ensure(
            vk.CreateSemaphore(device.handle, &semaphore_create_info, nil, &frame.swap_sem),
            msg = "Failed to create swap semaphore"
        )
        __ensure(
            vk.CreateSemaphore(device.handle, &semaphore_create_info, nil, &frame.render_sem),
            msg = "Failed to create render semaphore"
        )

        //_____________________________
        // Descriptor Pool
        size := vk.DescriptorPoolSize {
            type            = .UNIFORM_BUFFER, 
            descriptorCount = 1,
        }
        desc_pool_info := vk.DescriptorPoolCreateInfo {
            sType         = .DESCRIPTOR_POOL_CREATE_INFO,
            flags         = {},
            maxSets       = 1,
            poolSizeCount = 1,
            pPoolSizes    = &size,
        }
        __ensure(
            vk.CreateDescriptorPool(device.handle, &desc_pool_info, nil, &desc_pool),
            msg = "Failed to create frame descriptor pool"
        )

        //_____________________________
        // Descriptor Layout
        binding := vk.DescriptorSetLayoutBinding {
            binding         = 0, 
            descriptorType  = .UNIFORM_BUFFER, 
            descriptorCount = 1, 
            stageFlags      = { .VERTEX, .FRAGMENT }
        }
        set_layout_info := vk.DescriptorSetLayoutCreateInfo {
            sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            flags        = {},
            bindingCount = 1,
            pBindings    = &binding,
        }
        layout: vk.DescriptorSetLayout
        vk.CreateDescriptorSetLayout(device.handle, &set_layout_info, nil, &layout)

        //_____________________________
        // Allocate Descriptor Set
        allocate_info := vk.DescriptorSetAllocateInfo {
            sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
            descriptorPool     = desc_pool,
            descriptorSetCount = 1,
            pSetLayouts        = &layout
        }
        vk.AllocateDescriptorSets(device.handle, &allocate_info, &frame.descriptor.set)

        //_____________________________
        // Update Descriptor Data
        gpu_scene_data = gpu_scene_data_t {
            view          = glm.mat4(1),
            proj          = glm.mat4(1),
            sun_color     = { .4, .4, .6, 1  },
            ambient_color = {  1,  1,  1, 1  },
            sun_direction = {  0,  3,  5  },
            view_pos      = get_camera_pos(),
        }
        scene_allocation = create_buffer(size_of(gpu_scene_data_t), { .UNIFORM_BUFFER }, .CPU_TO_GPU)
        scene_unifrom_data := cast(^gpu_scene_data_t)scene_allocation.info.pMappedData
        scene_unifrom_data = &gpu_scene_data

        scene_descriptor = descriptor_data_t { set = frame.descriptor.set }
        scene_writes := []vk.WriteDescriptorSet {
            write_descriptor(&scene_descriptor, 0, .UNIFORM_BUFFER, descriptor_buffer_info_t { scene_allocation, size_of(gpu_scene_data_t), 0 }),
        }
        vk.UpdateDescriptorSets(device.handle, u32(len(scene_writes)), raw_data(scene_writes), 0, nil)
    }

    //_____________________________
    // Immediate Submit

    pool_create_info := vk.CommandPoolCreateInfo {
        sType            = .COMMAND_POOL_CREATE_INFO,
        queueFamilyIndex = queue.family,
        flags            = { .RESET_COMMAND_BUFFER },
    }
    __ensure(
        vk.CreateCommandPool(device.handle, &pool_create_info, nil, &immediate.pool),
        msg = "Failed to create command pool"
    )

    cmd_create_info := vk.CommandBufferAllocateInfo {
        sType              = .COMMAND_BUFFER_ALLOCATE_INFO,
        commandPool        = immediate.pool,
        level              = .PRIMARY,
        commandBufferCount = 1
    }
    __ensure(
        vk.AllocateCommandBuffers(device.handle, &cmd_create_info, &immediate.cmd), 
        msg = "Failed to create command buffer"
    )

    fence_create_info := vk.FenceCreateInfo {
        sType = .FENCE_CREATE_INFO,
        flags = { .SIGNALED }
    }
    __ensure(
        vk.CreateFence(device.handle, &fence_create_info, nil, &immediate.fence), 
        msg = "Failed to create fence"
    )

    //_____________________________
    // Descriptor Pool
    sizes := []vk.DescriptorPoolSize {
        { type = .STORAGE_IMAGE,          descriptorCount = 1 },
        { type = .UNIFORM_BUFFER,         descriptorCount = 3 },
        { type = .COMBINED_IMAGE_SAMPLER, descriptorCount = 5 },
    }
    desc_pool_info := vk.DescriptorPoolCreateInfo {
        sType         = .DESCRIPTOR_POOL_CREATE_INFO,
        flags         = {},
        maxSets       = 16,
        poolSizeCount = u32(len(sizes)),
        pPoolSizes    = raw_data(sizes),
    }
    __ensure(
        vk.CreateDescriptorPool(device.handle, &desc_pool_info, nil, &desc_pool), 
        msg = "Failed to create descriptor pool"
    )

    //_____________________________
    // Descriptor Layout
    img_info := vk.DescriptorImageInfo {
        sampler     = sampler,
        imageView   = draw_img.view,
        imageLayout = .GENERAL,
    }

    bindings: []vk.DescriptorSetLayoutBinding = {
        { binding = 0, descriptorType = .STORAGE_IMAGE, descriptorCount = 1, stageFlags = { .COMPUTE } }
    }
    set_layout_info := vk.DescriptorSetLayoutCreateInfo {
        sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        flags        = {},
        bindingCount = u32(len(bindings)),
        pBindings    = raw_data(bindings)
    }
    layout: vk.DescriptorSetLayout
    vk.CreateDescriptorSetLayout(device.handle, &set_layout_info, nil, &layout)

    //_____________________________
    // Descriptor Set
    allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &layout
    }
    vk.AllocateDescriptorSets(device.handle, &allocate_info, &draw_descriptor)

    draw_write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = draw_descriptor,
        dstBinding      = 0,
        descriptorCount = 1,
        descriptorType  = .STORAGE_IMAGE,
        pImageInfo      = &img_info,
    }
    vk.UpdateDescriptorSets(device.handle, 1, &draw_write, 0, nil)

    //_____________________________
    // Sampler
    sampler_info := vk.SamplerCreateInfo {
        sType     = .SAMPLER_CREATE_INFO,
        magFilter = .LINEAR,
        minFilter = .LINEAR,
    }
    __ensure(
        vk.CreateSampler(device.handle, &sampler_info, nil, &sampler), 
        msg = "Failed to create sampler"
    )

    sampler_write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = bindless.set,
        dstBinding      = 2,
        descriptorType  = .SAMPLER,
        pImageInfo      = &vk.DescriptorImageInfo { sampler = sampler },
        descriptorCount = 1,
    }
    vk.UpdateDescriptorSets(device.handle, 1, &sampler_write, 0, nil)

    //_____________________________
    // Error image
    black   : u32 = 0x00000000
    magenta : u32 = 0x00FF00FF
    pixels: [16 * 16]u32
    for x := 0; x < 16; x += 1 {
        for y := 0; y < 16; y += 1 {
            pixels[y * 16 + x] = bool(((x % 2) ~ (y % 2))) ? magenta : black
        }
    }
    error_image = create_image_from_buffer(&pixels, { 16, 16, 1 }, .R8G8B8A8_UNORM, { .SAMPLED })
    error_image.index = register_image(error_image.view)

    //_____________________________
    // Graphics Pipeline
    scene_data_layout_bindings := []vk.DescriptorSetLayoutBinding {
        vk.DescriptorSetLayoutBinding {
            binding         = 0,
            descriptorType  = .UNIFORM_BUFFER,
            descriptorCount = 1,
            stageFlags      = { .VERTEX, .FRAGMENT },
        },
    }
    scene_data_layout_info := vk.DescriptorSetLayoutCreateInfo {
        sType        = .DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        flags        = {},
        bindingCount = u32(len(scene_data_layout_bindings)),
        pBindings    = raw_data(scene_data_layout_bindings),
    }
    __ensure(
        vk.CreateDescriptorSetLayout(device.handle, &scene_data_layout_info, nil, &scene_data.set_layout), 
        msg = "Failed to create descriptor set"
    )
    scene_data_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &scene_data.set_layout,
    }
    __ensure(
        vk.AllocateDescriptorSets(device.handle, &scene_data_allocate_info, &scene_data.set), 
        msg = "Failed to allocate descriptor set"
    )

    __ensure(
        slang.createGlobalSession(slang.API_VERSION, &global_session),
        msg = "Failed to create global slang session"
    )

    init_camera()

    //_____________________________
    // Shaders
    pbr_declare(global_session) // TODO: make switching easier
    //ui_declare()
    //rect_declare(global_session)

    //_____________________________
    // imgui
    init_imgui(&swapchain_create_info)

    vk.GetPhysicalDeviceProperties(device.physical, &device.properties)
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
rebuild_swapchain :: proc() {
    defer resize_window = false
    __ensure(
        vk.QueueWaitIdle(queue.handle), 
        msg = "Waiting for queue failed"
    )

    vk.DestroySwapchainKHR(device.handle, swapchain.handle, nil)

    capabilities: vk.SurfaceCapabilitiesKHR
    vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical, surface, &capabilities)

    width, height := glfw.GetFramebufferSize(window)
    swapchain.extent = vk.Extent2D { u32(width), u32(height) }

    swapchain_create_info := vk.SwapchainCreateInfoKHR {
        sType            = .SWAPCHAIN_CREATE_INFO_KHR,
        presentMode      = .IMMEDIATE,
        compositeAlpha   = { .OPAQUE },
        imageArrayLayers = 1,
        imageColorSpace  = .SRGB_NONLINEAR,
        surface          = surface,
        imageUsage       = { .TRANSFER_DST, .COLOR_ATTACHMENT },
        imageFormat      = .B8G8R8A8_UNORM,
        preTransform     = capabilities.currentTransform,
        imageExtent      = swapchain.extent,
        minImageCount    = capabilities.minImageCount + 1,
    }
    __ensure(
        vk.CreateSwapchainKHR(device.handle, &swapchain_create_info, nil, &swapchain.handle), 
        msg = "Failed to create swapchain"
    )

    //_____________________________
    // Images
    swapchain_img_count: u32
    vk.GetSwapchainImagesKHR(device.handle, swapchain.handle, &swapchain_img_count, nil)
    swapchain.images = make([dynamic]vk.Image, swapchain_img_count)
    vk.GetSwapchainImagesKHR(device.handle, swapchain.handle, &swapchain_img_count, raw_data(swapchain.images))

    for i: u32 = 0; i < swapchain_img_count; i += 1 {
        img_view: vk.ImageView
        img_view_create_info := vk.ImageViewCreateInfo {
            sType            = .IMAGE_VIEW_CREATE_INFO,
            viewType         = .D2,
            format           = .B8G8R8A8_UNORM,
            image            = swapchain.images[i],
            subresourceRange = {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            }
        }
        vk.CreateImageView(device.handle, &img_view_create_info, nil, &img_view)
        append(&swapchain.views, img_view)
    }
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
create_shader_module :: proc(path: string) -> (shader: vk.ShaderModule) {
    data, result := os.read_entire_file(path)
    __ensure(result, "Failed to open file")
    shader_module_info := vk.ShaderModuleCreateInfo {
        sType    = .SHADER_MODULE_CREATE_INFO,
        codeSize = len(data),
        pCode    = cast(^u32) raw_data(data)
    }
    __ensure(
        vk.CreateShaderModule(device.handle, &shader_module_info, nil, &shader), 
        msg = "Failed to create shader module"
    )
    return shader
}

write_descriptor /* +-+-+-+-+ */ :: proc(
    data     : ^descriptor_data_t,
    binding  : u32, 
    type     : vk.DescriptorType, 
    resource : descriptor_info_t,
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> (write: vk.WriteDescriptorSet) {
    write = {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = data.set,
        dstBinding      = binding,
        descriptorCount = 1,
        descriptorType  = type,
    }
    switch info in resource {
        case descriptor_image_info_t:  {
            img_info := vk.DescriptorImageInfo {
                sampler      = info.sampler,
                imageView    = info.view,
                imageLayout  = info.layout,
            }
            if len(data.buffer_info) == 0 do append(&data.image_info, img_info)
            //i := len(data.image_info) - 1
            write.pImageInfo = &data.image_info[0]
        }
        case descriptor_buffer_info_t: {
            buffer_info := vk.DescriptorBufferInfo {
                buffer = info.buffer.handle,
                offset = info.offset,
                range  = info.size,
            }
            if len(data.buffer_info) == 0 do append(&data.buffer_info, buffer_info)
            //i := len(data.buffer_info) - 1
            write.pBufferInfo = &data.buffer_info[0]
        }
    }
    return write
}

upload_mesh /* +-+-+-+-+-+-+-+ */ :: proc(
    vertex_buffer : []vertex_t,
    index_buffer  : []u32,
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> (data: render_data_t) {
    data = { 
        //material = &pbr,
        first = 0,
        count = u32(len(index_buffer)),
    }
    //sigil.add(entity, data)
    //sigil.add(entity, transform_t(0))

    vtx_buffer_size := vk.DeviceSize(len(vertex_buffer) * size_of(vertex_t))
    idx_buffer_size := vk.DeviceSize(len(index_buffer)  * size_of(u32))

    vertex := create_buffer(vtx_buffer_size, { .STORAGE_BUFFER, .TRANSFER_DST, .SHADER_DEVICE_ADDRESS }, .GPU_ONLY)
    index  := create_buffer(idx_buffer_size, { .INDEX_BUFFER, .TRANSFER_DST }, .GPU_ONLY)

    device_address_info := vk.BufferDeviceAddressInfo {
        sType  = .BUFFER_DEVICE_ADDRESS_INFO,
        buffer = vertex.handle,
    }
    address := vk.GetBufferDeviceAddress(device.handle, &device_address_info)

    min_pos := glm.vec3 { math.F32_MAX, math.F32_MAX, math.F32_MAX }
    max_pos := glm.vec3 { math.F32_MIN, math.F32_MIN, math.F32_MIN }
    for v in vertex_buffer {
        min_pos = glm.min(min_pos, v.position)
        max_pos = glm.max(max_pos, v.position)
    }
    center := (min_pos + max_pos) * 0.5
    radius := glm.length(max_pos - center)
    
    data.address    = address
    data.idx_buffer = index.handle

    staging_buffer := create_buffer(vtx_buffer_size + idx_buffer_size, { .TRANSFER_SRC }, .CPU_ONLY)
    defer destroy_buffer(staging_buffer)
    buffer_data := staging_buffer.info.pMappedData
    mem.copy(buffer_data, raw_data(vertex_buffer), int(vtx_buffer_size))
	mem.copy(mem.ptr_offset((^u8)(buffer_data), vtx_buffer_size), raw_data(index_buffer), int(idx_buffer_size))

    __ensure(
        vk.ResetFences(device.handle, 1, &immediate.fence), 
        msg = "Failed resetting immediate fence"
    )
    __ensure(
        vk.ResetCommandBuffer(immediate.cmd, {}), 
        msg = "Failed to reset command buffer"
    )

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }
    __ensure(vk.BeginCommandBuffer(immediate.cmd, &cmd_begin_info), msg = "Failed to being immediate command buffer")
    {
        debug_label := vk.DebugUtilsLabelEXT {
            sType      = .DEBUG_UTILS_LABEL_EXT,
            pLabelName = "upload mesh",
            color      = { 0, 255, 0, 255 },
        }
        vk.CmdBeginDebugUtilsLabelEXT(immediate.cmd, &debug_label)
        vtx_copy := vk.BufferCopy {
            srcOffset = 0,
            dstOffset = 0,
            size      = vtx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, vertex.handle, 1, &vtx_copy)
        idx_copy := vk.BufferCopy {
            srcOffset = vtx_buffer_size,
            dstOffset = 0,
            size      = idx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, index.handle, 1, &idx_copy)
        vk.CmdEndDebugUtilsLabelEXT(immediate.cmd)
    }
    __ensure(vk.EndCommandBuffer(immediate.cmd), msg = "Failed to end immediate command buffer")

    cmd_info := vk.CommandBufferSubmitInfo {
        sType         = .COMMAND_BUFFER_SUBMIT_INFO,
        commandBuffer = immediate.cmd,
        deviceMask    = 0,
    }
    submit_info := vk.SubmitInfo2 {
        sType                  = .SUBMIT_INFO_2,
        commandBufferInfoCount = 1,
        pCommandBufferInfos    = &cmd_info,
    }
    __ensure(
        vk.QueueSubmit2(queue.handle, 1, &submit_info, immediate.fence), 
        msg = "Failed to sumbit queue"
    )
    __ensure(
        vk.WaitForFences(device.handle, 1, &immediate.fence, true, max(u64)), 
        msg = "Failed waiting for fences"
    )
    return
}

parse_gltf_scene :: proc(path: cstring) -> (created: [dynamic]sigil.entity_t) {
    //if !os.is_file_path(string(path)) {
    //    fmt.printfln("path %s is not a file path", path)
    //    return {}, {}
    //}

    file_data, _ := cgltf.parse_file({}, path)
    defer cgltf.free(file_data)
    __ensure(
        cgltf.load_buffers({}, file_data, path),
        msg = fmt.aprintf("Failed to load buffers from path '%s'", path, allocator = context.temp_allocator)
    )
    //fmt.println(file_data.json)
    for scene in file_data.scenes {
        //fmt.println(scene.name)
        for node in scene.nodes {
            //fmt.println(node.name)
            e := sigil.new_entity()
            append(&created, e)
            //fmt.println(created)
            if node.mesh != nil {
                sigil.add(e, sigil.name_t(node.name))

                p := glm.vec3 { node.translation.x, node.translation.y, node.translation.z }
                pos := glm.mat4Translate(glm.vec3(0))
                if node.has_translation {
                    pos = glm.mat4Translate(p)
                }
                rot := glm.quat(1)
                if node.has_rotation {
                    rot = transmute(quaternion128)node.rotation
                }

                scl := glm.mat4Translate(glm.vec3(0))
                if node.has_scale {
                    scl = glm.mat4Scale(glm.vec3 { node.scale.x, node.scale.y, node.scale.z })
                }
                model := pos * glm.mat4FromQuat(rot) * scl
                sigil.add(e, transform_t(model))

                d := parse_gltf_mesh(node.mesh^)
                r_data := upload_mesh(d.vertices, d.indices)
                sigil.add(e, r_data)

                json_get_obj :: proc(value: json.Value, key: string) -> json.Value {
                    if obj, valid := value.(json.Object); valid do if ret, valid := obj[key]; valid do return ret; return nil
                }

                for i in 0..<node.extensions_count {
                    ext := node.extensions[i]
                    //fmt.println(ext.name)
                    if ext.name == "KHR_collision_shapes" {
                        //fmt.println("shapes")
                        //json_str := strings.clone_from_cstring(auto_cast ext.data)
                        //fmt.println(json_str)
                    }
                    if ext.name == "KHR_physics_rigid_bodies" {
                        //fmt.println("physics")
                        json_str := strings.clone_from_cstring(auto_cast ext.data)
                        defer delete(json_str)
                        //fmt.println(json_str)
                        parsed, err := json.parse(transmute([]u8)json_str)
                        //fmt.println(parsed)
                        collider := json_get_obj(parsed, "collider")
                        geometry := json_get_obj(collider, "geometry")
                        shape := json_get_obj(geometry, "shape")
                        if value, exists := shape.(json.Float); exists {
                            
                            //fmt.println("Found shape:", value)
                            switch value {
                                // these change with more shapes, need to figure out exporting KHR_collision_shapes proper
                                case 0: { // sphere
                                    add_physics_shape(e, &p, &rot, &node.scale)
                                }
                                case 1: { // box
                                    add_physics_shape(e, &p, &rot, &node.scale, auto_cast 0)
                                }
                                case 2: {
                                    add_physics_shape(e, &p, &rot, &node.scale, auto_cast 0)
                                }
                                case 3: {
                                    add_physics_shape(e, &p, &rot, &node.scale)
                                }
                                default: {}
                            }
                        }
                    }
                }
            }
        }
    }
    return
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
parse_gltf_mesh :: proc(m: cgltf.mesh) -> (mesh_data: mesh_data_t) { // only a single mesh rn effectivly, gotta sort that in the future sometime
    vertex_buffer: [dynamic]vertex_t
    index_buffer:  [dynamic]u32
    for prim in m.primitives {
        idx_accessor := prim.indices
        idx_buffer := cast([^]u8)idx_accessor.buffer_view.buffer.data
        idx_offset := idx_accessor.offset + idx_accessor.buffer_view.offset

        idx_count := int(idx_accessor.count)
        resize(&index_buffer, len(index_buffer) + int(idx_count))


        idx_len := len(index_buffer) - int(idx_count)
        #partial switch idx_accessor.component_type {
            case .r_16, .r_16u:
                src := mem.slice_ptr(cast([^]u16)(&idx_buffer[idx_offset]), int(idx_count))
                for i in 0 ..< idx_count {
                    index_buffer[idx_len + i] = u32(src[i])
                }
            case .r_32u:
                src := mem.slice_ptr(cast([^]u32)(&idx_buffer[idx_offset]), int(idx_count))
                copy(index_buffer[idx_len:], src[:idx_count])
            case:
                fmt.eprintln("Unsupported index type:", idx_accessor.component_type)
        }
        
        pos_accessor: ^cgltf.accessor
        nor_accessor: ^cgltf.accessor
        tex_accessor: ^cgltf.accessor
        col_accessor: ^cgltf.accessor
        for attr in prim.attributes do #partial switch attr.type {
            case .position: pos_accessor = attr.data
            case .normal:   nor_accessor = attr.data
            case .texcoord: tex_accessor = attr.data
            case .color:    col_accessor = attr.data
        }

        vertex_count := int(pos_accessor.count)
        old_len := len(vertex_buffer)
        resize(&vertex_buffer, old_len + int(vertex_count))

        if pos_accessor != nil {
            pos_buffer := cast([^]u8)pos_accessor.buffer_view.buffer.data
            pos_offset := pos_accessor.offset + pos_accessor.buffer_view.offset
            pos_src := mem.slice_ptr(cast([^][3]f32)(&pos_buffer[pos_offset]), int(vertex_count))

            for i in 0 ..< vertex_count {
                vertex_buffer[old_len + i].position = pos_src[i]
            }
        }

        if nor_accessor != nil {
            norm_buffer := cast([^]u8)nor_accessor.buffer_view.buffer.data
            norm_offset := nor_accessor.offset + nor_accessor.buffer_view.offset
            norm_src    := mem.slice_ptr(cast([^][3]f32)(&norm_buffer[norm_offset]), int(vertex_count))

            for i in 0 ..< vertex_count {
                vertex_buffer[old_len + i].normal = norm_src[i]
            }
        }

        if tex_accessor != nil {
            uv_buffer := cast([^]u8)tex_accessor.buffer_view.buffer.data
            uv_offset := tex_accessor.offset + tex_accessor.buffer_view.offset
            uv_src    := mem.slice_ptr(cast([^][2]f32)(&uv_buffer[uv_offset]), int(vertex_count))

            for i in 0 ..< vertex_count {
                vertex_buffer[old_len + i].uv_x = uv_src[i].x
                vertex_buffer[old_len + i].uv_y = uv_src[i].y
            }
        }

        if col_accessor != nil {
            col_buffer := cast([^]u8)col_accessor.buffer_view.buffer.data
            col_offset := col_accessor.offset + col_accessor.buffer_view.offset
            col_src    := mem.slice_ptr(cast([^][4]f32)(&col_buffer[col_offset]), int(vertex_count))

            for i in 0 ..< vertex_count {
                vertex_buffer[old_len + i].color = col_src[i]
            }
        }

        //prim.material.pbr_metallic_roughness.base_color_texture
    }
    if len(vertex_buffer) > 0 && len(index_buffer) > 0 {
        success := generate_tangents_for_mesh(vertex_buffer[:], index_buffer[:])
        if !success {
            ensure_default_tangents(vertex_buffer[:])
            fmt.println("Failed to generate tangets for mesh")
        }
    }
    return { vertex_buffer[:], index_buffer[:] }
}

MikkContext :: struct {
    vertices: []vertex_t,
    indices: []u32,
    face_count: int,
}

// MikkTSpace interface implementations
get_num_faces :: proc(pContext: ^mikktspace.Context) -> int {
    ctx := cast(^MikkContext)pContext.user_data
    return ctx.face_count
}

get_num_vertices_of_face :: proc(pContext: ^mikktspace.Context, iFace: int) -> int {
    return 3 // We only handle triangles
}

get_position :: proc(pContext: ^mikktspace.Context, iFace: int, iVert: int) -> [3]f32 {
    ctx := cast(^MikkContext)pContext.user_data
    idx := ctx.indices[iFace * 3 + iVert]
    vertex := &ctx.vertices[idx]
    return vertex.position
}

get_normal :: proc(pContext: ^mikktspace.Context, iFace: int, iVert: int) -> [3]f32 {
    ctx := cast(^MikkContext)pContext.user_data
    idx := ctx.indices[iFace * 3 + iVert]
    vertex := &ctx.vertices[idx]
    return vertex.normal
}

get_tex_coord :: proc(pContext: ^mikktspace.Context, iFace: int, iVert: int) -> [2]f32 {
    ctx := cast(^MikkContext)pContext.user_data
    idx := ctx.indices[iFace * 3 + iVert]
    vertex := &ctx.vertices[idx]
    return {vertex.uv_x, vertex.uv_y}
}

set_t_space_basic :: proc(pContext: ^mikktspace.Context, fvTangent: [3]f32, fSign: f32, iFace: int, iVert: int) {
    ctx := cast(^MikkContext)pContext.user_data
    idx := ctx.indices[iFace * 3 + iVert]
    vertex := &ctx.vertices[idx]
    
    vertex.tangent = {fvTangent.x, fvTangent.y, fvTangent.z, fSign}
}

generate_tangents_for_mesh :: proc(vertices: []vertex_t, indices: []u32) -> bool {
    if len(vertices) == 0 || len(indices) == 0 || len(indices) % 3 != 0 {
        return false
    }

    context_data := MikkContext{
        vertices = vertices,
        indices = indices,
        face_count = len(indices) / 3,
    }

    interface := mikktspace.Interface{
        get_num_faces = get_num_faces,
        get_num_vertices_of_face = get_num_vertices_of_face,
        get_position = get_position,
        get_normal = get_normal,
        get_tex_coord = get_tex_coord,
        set_t_space_basic = set_t_space_basic,
        set_t_space = nil, // We're using basic tangent space
    }

    mikk_context := mikktspace.Context{
        interface = &interface,
        user_data = &context_data,
    }

    return mikktspace.generate_tangents(&mikk_context)
}

// Helper to ensure all vertices have valid tangents (fallback for failed generation)
ensure_default_tangents :: proc(vertices: []vertex_t) {
    for i in 0..<len(vertices) {
        if vertices[i].tangent == {0, 0, 0, 0} {
            // Default tangent (1, 0, 0) with positive handedness
            vertices[i].tangent = {1, 0, 0, 1}
        }
    }
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
load_mesh :: proc(path: cstring) -> (data: [dynamic]render_data_t) {
    //meshes := parse_gltf_scene(path)
    //for mesh in meshes {
    //    rd := upload_mesh(mesh.vertices, mesh.indices)
    //    append(&data, rd)
    //}
    return
}

create_buffer /* +-+-+-+-+-+ */ :: proc(
    buffer_size  : vk.DeviceSize, 
    buffer_usage : vk.BufferUsageFlags, 
    mem_usage    : vma.MemoryUsage
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> (buffer: allocated_buffer_t) {
    buffer_info := vk.BufferCreateInfo {
        sType = .BUFFER_CREATE_INFO,
        size  = buffer_size,
        usage = buffer_usage,
    }
    alloc_info := vma.AllocationCreateInfo {
        flags = { .MAPPED },
        usage = mem_usage,
    }
    __ensure(
        vma.CreateBuffer(vma_allocator, &buffer_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info), 
        msg = "Failed to create buffer"
    )
    //if buffer_usage == { .STORAGE_BUFFER } do buffer.index = register_buffer(buffer.handle, buffer.info.size)
    return buffer
}

register_buffer /* +-+ */ :: proc(
    buffer : vk.Buffer,
    size   : vk.DeviceSize,
/* +-+-+-+-+-+-+-+-+-+ */ ) -> u32 {
    if index, ok := buffer_indices[buffer]; ok {
        return index
    }
    index := u32(len(buffers))
    append(&buffers, buffer)
    buffer_indices[buffer] = index

    buffer_info := vk.DescriptorBufferInfo {
        buffer = buffer,
        offset = 0,
        range  = size,
    }
    write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = bindless.set,
        dstBinding      = 0,
        //dstArrayElement = index,
        descriptorType  = .STORAGE_BUFFER,
        pBufferInfo     = &buffer_info,
        descriptorCount = 1,
    }
    vk.UpdateDescriptorSets(device.handle, 1, &write, 0, nil)
    return index
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
destroy_buffer :: proc(buffer: allocated_buffer_t) {
    vma.DestroyBuffer(vma_allocator, buffer.handle, buffer.allocation)
}

load_image /* +-+-+-+-+-+ */ :: proc(
    format : vk.Format, 
    usage  : vk.ImageUsageFlags,
    path   : cstring
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> allocated_image_t {
    width, height: i32
    pixels := image.load(path, &width, &height, nil, 4 /* STBI_rgb_alpha */)
    if pixels == nil {
        __log("Failed to load image, path: ", path)
        return error_image
    }
    return create_image_from_buffer(pixels, { u32(width), u32(height), 1 }, .R8G8B8A8_UNORM, { .SAMPLED })
}

create_image /* +-+-+-+-+-+ */ :: proc(
    format : vk.Format,
    flags  : vk.ImageUsageFlags,
    extent : vk.Extent3D
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> allocated_image_t {
    alloc_img := allocated_image_t { extent = extent, format = format }

    img_info := vk.ImageCreateInfo {
        sType       = .IMAGE_CREATE_INFO,
        imageType   = .D2,
        format      = format,
        extent      = extent,
        mipLevels   = 1,
        arrayLayers = 1,
        samples     = { ._1 },
        tiling      = .OPTIMAL,
        usage       = flags,
    }
    alloc_info := vma.AllocationCreateInfo {
        usage         = .GPU_ONLY,
        requiredFlags = { .DEVICE_LOCAL },
    }
    __ensure(
        vma.CreateImage(vma_allocator, &img_info, &alloc_info, &alloc_img.handle, &alloc_img.allocation, nil), 
        msg = "Failed to create image"
    )

    view_info := vk.ImageViewCreateInfo {
        sType            = .IMAGE_VIEW_CREATE_INFO,
        image            = alloc_img.handle,
        viewType         = .D2,
        format           = alloc_img.format,
        subresourceRange = {
            aspectMask      = (format == .D32_SFLOAT) ? { .DEPTH } : { .COLOR },
            baseMipLevel    = 0,
            levelCount      = 1,
            baseArrayLayer  = 0,
            layerCount      = 1,
        }
    }
    __ensure(
        vk.CreateImageView(device.handle, &view_info, nil, &alloc_img.view), 
        msg = "Failed to create image view"
    )
    //alloc_img.index = register_image(alloc_img.view)
    return alloc_img
}

create_image_from_buffer /* + */ :: proc(
    data   : rawptr,
    extent : vk.Extent3D,
    format : vk.Format,
    usage  : vk.ImageUsageFlags
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) -> allocated_image_t {
    size: vk.DeviceSize = vk.DeviceSize(extent.depth * extent.width * extent.height * 4)
    upload_buffer := create_buffer(size, { .TRANSFER_SRC }, .CPU_TO_GPU)
    defer destroy_buffer(upload_buffer)
    mem.copy(upload_buffer.info.pMappedData, data, int(size))

    img := create_image(format, usage + { .TRANSFER_DST, .TRANSFER_SRC }, extent)

    __ensure(
        vk.ResetFences(device.handle, 1, &immediate.fence), 
        msg = "Failed resetting immediate fence"
    )
    __ensure(
        vk.ResetCommandBuffer(immediate.cmd, {}), 
        msg = "Failed to reset command buffer"
    )

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }

    __ensure(vk.BeginCommandBuffer(immediate.cmd, &cmd_begin_info), "Failed to being immediate command buffer")
    {
        transition_img(immediate.cmd, img.handle, .UNDEFINED, .TRANSFER_DST_OPTIMAL)
        copy := vk.BufferImageCopy {
            bufferOffset      = 0,
            bufferRowLength   = 0,
            bufferImageHeight = 0,
            imageExtent       = extent,
            imageSubresource  = {
                aspectMask      = { .COLOR },
                mipLevel        = 0,
                baseArrayLayer  = 0,
                layerCount      = 1,
            }
        }
        vk.CmdCopyBufferToImage(immediate.cmd, upload_buffer.handle, img.handle, .TRANSFER_DST_OPTIMAL, 1, &copy)
        transition_img(immediate.cmd, img.handle, .TRANSFER_DST_OPTIMAL, .SHADER_READ_ONLY_OPTIMAL)
    }
    __ensure(vk.EndCommandBuffer(immediate.cmd), "Failed to end immediate command buffer")

    cmd_info := vk.CommandBufferSubmitInfo {
        sType         = .COMMAND_BUFFER_SUBMIT_INFO,
        commandBuffer = immediate.cmd,
        deviceMask    = 0,
    }
    submit_info := vk.SubmitInfo2 {
        sType                  = .SUBMIT_INFO_2,
        commandBufferInfoCount = 1,
        pCommandBufferInfos    = &cmd_info,
    }
    __ensure(
        vk.QueueSubmit2(queue.handle, 1, &submit_info, immediate.fence), 
        msg = "Failed to sumbit queue"
    )
    __ensure(
        vk.WaitForFences(device.handle, 1, &immediate.fence, true, max(u64)), 
        msg = "Failed waiting for fences"
    )
    return img
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
register_image :: proc(image_view: vk.ImageView) -> u32 {
    if index, ok := texture_indices[image_view]; ok {
        return index
    }
    index := u32(len(textures))
    append(&textures, image_view)
    texture_indices[image_view] = index

    image_info := vk.DescriptorImageInfo {
        sampler     = sampler,
        imageView   = image_view,
        imageLayout = .SHADER_READ_ONLY_OPTIMAL,
    }
    write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = bindless.set,
        dstBinding      = 1,
        dstArrayElement = index,
        descriptorType  = .SAMPLED_IMAGE,
        pImageInfo      = &image_info,
        descriptorCount = 1,
    }
    vk.UpdateDescriptorSets(device.handle, 1, &write, 0, nil)
    return index
}

transition_img /* +-+-+-+-+-+ */ :: proc(
    cmd  : vk.CommandBuffer,
    img  : vk.Image,
    from : vk.ImageLayout,
    to   : vk.ImageLayout
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) {
    img_barrier := vk.ImageMemoryBarrier2 {
        sType         = .IMAGE_MEMORY_BARRIER_2,
        srcStageMask  = { .ALL_COMMANDS },
        srcAccessMask = { .MEMORY_WRITE },
        dstStageMask  = { .ALL_COMMANDS },
        dstAccessMask = { .MEMORY_WRITE, .MEMORY_READ },
        oldLayout     = from,
        newLayout     = to,
        image         = img,
        subresourceRange = {
            aspectMask     = (to == .DEPTH_ATTACHMENT_OPTIMAL) ? { .DEPTH } : { .COLOR },
            baseMipLevel   = 0,
            levelCount     = vk.REMAINING_MIP_LEVELS,
            baseArrayLayer = 0,
            layerCount     = vk.REMAINING_ARRAY_LAYERS,
        }
    }
    dependency_info := vk.DependencyInfo {
        sType                   = .DEPENDENCY_INFO,
        imageMemoryBarrierCount = 1,
        pImageMemoryBarriers    = &img_barrier,
    }
    vk.CmdPipelineBarrier2(cmd, &dependency_info)
}

blit_imgs /* +-+-+-+-+-+-+-+ */ :: proc(
    cmd        : vk.CommandBuffer,
    src        : vk.Image,
    dst        : vk.Image,
    src_extent : vk.Extent2D,
    dst_extent : vk.Extent2D
/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */ ) {
    blit_region := vk.ImageBlit2 {
        sType          = .IMAGE_BLIT_2,
        srcSubresource = {
            aspectMask     = { .COLOR },
            mipLevel       = 0,
            baseArrayLayer = 0,
            layerCount     = 1,
        },
        srcOffsets = {
            vk.Offset3D { 0, 0, 0 },
            vk.Offset3D { i32(src_extent.width), i32(src_extent.height), 1},
        },
        dstSubresource = {
            aspectMask     = { .COLOR },
            mipLevel       = 0,
            baseArrayLayer = 0,
            layerCount     = 1,
        },
        dstOffsets = {
            vk.Offset3D { 0, 0, 0 },
            vk.Offset3D { i32(dst_extent.width), i32(dst_extent.height), 1},
        }
    }
    blit_info := vk.BlitImageInfo2 {
        sType          = .BLIT_IMAGE_INFO_2,
        srcImage       = src,
        srcImageLayout = .TRANSFER_SRC_OPTIMAL,
        dstImage       = dst,
        dstImageLayout = .TRANSFER_DST_OPTIMAL,
        regionCount    = 1,
        pRegions       = &blit_region,
        filter         = .LINEAR,
    }
    vk.CmdBlitImage2(cmd, &blit_info)
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
tick_vulkan :: proc() {
    if resize_window do rebuild_swapchain()

    frame := &frames[current_frame]
    err := vk.WaitForFences(device.handle, 1, &frame.fence, true, max(u64)); if err == .ERROR_DEVICE_LOST {
        __log("Wait for fences failed. Device Lost!")
        glfw.SetWindowShouldClose(window, true)
    }
    __ensure(
        vk.WaitForFences(device.handle, 1, &frame.fence, true, max(u64)), 
        msg = "WaitForFences failed"
    )

    //
    __rebuild_pbr_pipeline(global_session)
    //

    img_index: u32
    result := vk.AcquireNextImageKHR(device.handle, swapchain.handle, max(u64), frame.swap_sem, {}, &img_index)
    if result == .ERROR_OUT_OF_DATE_KHR || result == .SUBOPTIMAL_KHR { resize_window = true }
    swap_img := swapchain.images[img_index]

    __ensure(
        vk.ResetFences(device.handle, 1, &frame.fence), 
        msg = "Reset fence failed"
    )
    vk.ResetCommandBuffer(frame.cmd, {})

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT },
    }
    draw_extent.width  = math.min(swapchain.extent.width, draw_img.extent.width);
    draw_extent.height = math.min(swapchain.extent.width, draw_img.extent.height);
    __ensure(vk.BeginCommandBuffer(frame.cmd, &cmd_begin_info), "Begin cmd failed")
    {
        transition_img(frame.cmd, draw_img.handle, .UNDEFINED, .GENERAL)

        //________________
        // draw backgrund
        vk.CmdClearColorImage(
            frame.cmd,
            draw_img.handle,
            .GENERAL,
            &vk.ClearColorValue { float32 = { .066, .066, .066, 1 } },
            1,
            &vk.ImageSubresourceRange {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            },
        )

        transition_img(frame.cmd, draw_img.handle, .GENERAL, .COLOR_ATTACHMENT_OPTIMAL)
        transition_img(frame.cmd, depth_img.handle, .UNDEFINED, .DEPTH_ATTACHMENT_OPTIMAL)

        //________________
        // draw geometry
        draw_attach_info := vk.RenderingAttachmentInfo {
            sType       = .RENDERING_ATTACHMENT_INFO,
            imageView   = draw_img.view,
            imageLayout = .GENERAL,
            loadOp      = .LOAD,
            storeOp     = .STORE,
        }
        depth_attach_info := vk.RenderingAttachmentInfo {
            sType       = .RENDERING_ATTACHMENT_INFO,
            imageView   = depth_img.view,
            imageLayout = .DEPTH_ATTACHMENT_OPTIMAL,
            loadOp      = .CLEAR,
            storeOp     = .STORE,
            clearValue  = vk.ClearValue { depthStencil = { depth = 0.0 } },
        }
        render_info := vk.RenderingInfo {
            sType                = .RENDERING_INFO,
            layerCount           = 1,
            colorAttachmentCount = 1,
            pColorAttachments    = &draw_attach_info,
            pDepthAttachment     = &depth_attach_info,
            pStencilAttachment   = nil,
            renderArea           = {
                offset = { 0, 0 },
                extent = draw_extent,
            },
        }

        vk.CmdBeginRendering(frame.cmd, &render_info)
        {
            debug_label := vk.DebugUtilsLabelEXT {
                sType = .DEBUG_UTILS_LABEL_EXT,
                pLabelName = fmt.caprintf("Frame %d", current_frame, allocator = context.temp_allocator),
                color = { 0, 255, 0, 255 },
            }
            vk.CmdBeginDebugUtilsLabelEXT(frame.cmd, &debug_label)

            viewport := vk.Viewport {
                x        = 0,
                y        = 0,
                width    = f32(draw_extent.width),
                height   = f32(draw_extent.height),
                minDepth = 1,
                maxDepth = 0,
            }
            vk.CmdSetViewport(frame.cmd, 0, 1, &viewport)

            scissor := vk.Rect2D {
                offset = { 0, 0 },
                extent = draw_extent,
            }
            vk.CmdSetScissor(frame.cmd, 0, 1, &scissor)

            view       := get_camera_view()
            projection := get_camera_projection()

            gpu_scene_data.view = view
            gpu_scene_data.proj = projection
            gpu_scene_data.view_pos = get_camera_pos()
            gpu_scene_data.time = time
            mem.copy(scene_allocation.info.pMappedData, &gpu_scene_data, size_of(gpu_scene_data_t))

            scene_descriptor.set = frame.descriptor.set
            scene_data_writes := []vk.WriteDescriptorSet {
                write_descriptor(&scene_descriptor, 0, .UNIFORM_BUFFER, descriptor_buffer_info_t { scene_allocation, size_of(gpu_scene_data_t), 0 }),
            }
            vk.UpdateDescriptorSets(device.handle, u32(len(scene_data_writes)), raw_data(scene_data_writes), 0, nil)

            // todo: impl comp culling
            //mem.copy(object_buffer.info.pMappedData, &object_data, size_of(object_data))
            //vk.CmdBindPipeline(frame.cmd, .COMPUTE, cull_pipeline)
            //vk.CmdBindDescriptorSets(frame.cmd, .COMPUTE, cull_pipeline_layout, 0, 1, &cull_descriptor.set, 0, nil)
            //fmt.println(sigil.core.groups[sigil.types_hash(render_data_t, transform_t)])
            for &q, i in sigil.query(render_data_t, transform_t) {
                data, transform := q.x, glm.mat4(q.y)

                offset := u32(i * size_of(glm.mat4))
                mem.copy(rawptr(uintptr(transform_buffer.info.pMappedData) + uintptr(offset)), &transform, size_of(glm.mat4))

                // todo: need to set up material system to enable dynamic texturing for different meshes
                vk.CmdBindPipeline(frame.cmd, .GRAPHICS, pbr.pipeline)
                sets := []vk.DescriptorSet { frame.descriptor.set, pbr.set, bindless.set }
                vk.CmdBindDescriptorSets(frame.cmd, .GRAPHICS, pbr.pipeline_layout, 0, u32(len(sets)), raw_data(sets), 0, nil)
                //data.material.update_delegate(frame.cmd, data)
                pbr_push_const.model = u32(i) // transform idx
                pbr_push_const.vertex_buffer = data.address
                vk.CmdPushConstants(frame.cmd, pbr.pipeline_layout, { .VERTEX, .FRAGMENT }, 0, size_of(pbr_push_constant_t), &pbr_push_const)
                vk.CmdBindIndexBuffer(frame.cmd, data.idx_buffer, 0, .UINT32)

                vk.CmdDrawIndexed(frame.cmd, data.count, 1, data.first, 0, 0)
            }
            draw_ui(frame.cmd, swapchain.views[current_frame])

            vk.CmdEndDebugUtilsLabelEXT(frame.cmd)
        }
        vk.CmdEndRendering(frame.cmd)

        //________________
        // prepare for present
        transition_img(frame.cmd, draw_img.handle, .COLOR_ATTACHMENT_OPTIMAL, .TRANSFER_SRC_OPTIMAL)
        transition_img(frame.cmd, swap_img, .UNDEFINED, .TRANSFER_DST_OPTIMAL)

        blit_imgs(frame.cmd, draw_img.handle, swap_img, draw_extent, swapchain.extent)

        transition_img(frame.cmd, swap_img, .TRANSFER_DST_OPTIMAL, .PRESENT_SRC_KHR)
    }
    __ensure(
        vk.EndCommandBuffer(frame.cmd), 
        msg = "End cmd failed"
    )

    submit_info := vk.SubmitInfo2 {
        sType                    = .SUBMIT_INFO_2,
        waitSemaphoreInfoCount   = 1,
        pWaitSemaphoreInfos      = &vk.SemaphoreSubmitInfo {
            sType       = .SEMAPHORE_SUBMIT_INFO,
            semaphore   = frame.swap_sem,
            value       = 1,
            stageMask   = { .COLOR_ATTACHMENT_OUTPUT },
            deviceIndex = 0,
        },
        commandBufferInfoCount   = 1,
        pCommandBufferInfos      = &vk.CommandBufferSubmitInfo {
            sType         = .COMMAND_BUFFER_SUBMIT_INFO,
            commandBuffer = frame.cmd,
            deviceMask    = 0,
        },
        signalSemaphoreInfoCount = 1,
        pSignalSemaphoreInfos    = &vk.SemaphoreSubmitInfo {
            sType       = .SEMAPHORE_SUBMIT_INFO,
            semaphore   = frame.render_sem,
            value       = 1,
            stageMask   = { .ALL_GRAPHICS },
            deviceIndex = 0,
        },
    }
    __ensure(
        vk.QueueSubmit2(queue.handle, 1, &submit_info, frame.fence), 
        msg = "Queue submit failed"
    )

    present_info := vk.PresentInfoKHR {
        sType              = .PRESENT_INFO_KHR,
        waitSemaphoreCount = 1,
        pWaitSemaphores    = &frame.render_sem,
        pImageIndices      = &img_index,
        swapchainCount     = 1,
        pSwapchains        = &swapchain.handle,
    }
    res := vk.QueuePresentKHR(queue.handle, &present_info); if res == .ERROR_OUT_OF_DATE_KHR || res == .SUBOPTIMAL_KHR {
        resize_window = true
    }
    current_frame = (current_frame >= (len(frames) - 1)) ? 0 : current_frame + 1
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
terminate_vulkan :: proc() {
    //__ensure(vk.DeviceWaitIdle(device.handle))

    //vk.DestroyImage(device.handle, draw_img.handle, nil)
    //vk.DestroyImageView(device.handle, draw_img.view, nil)

    //for img in swapchain.images do vk.DestroyImage(device.handle, img, nil)
    //for view in swapchain.views      do vk.DestroyImageView(device.handle, view, nil)

    //vk.DestroySwapchainKHR(device.handle, swapchain, nil)

    //// destroy the rest

    //free_imgui()

    //for &frame in frames {
    //    vk.DestroyCommandPool(device.handle, frame.pool, nil)
    //    vk.DestroyFence(device.handle, frame.fence, nil)
    //    vk.DestroySemaphore(device.handle, frame.swap_sem, nil)
    //    vk.DestroySemaphore(device, frame.render_sem, nil)
    //}
    //vk.DestroyCommandPool(device.handle, immediate.pool, nil)
    //vk.DestroyFence(device.handle, immediate.fence, nil)

    //vk.DestroySurfaceKHR(instance, surface, nil)
    //vk.DestroyDevice(device.handle, nil)
    //vk.DestroyDebugUtilsMessengerEXT(instance, dbg_messenger, nil)
    //vk.DestroyInstance(instance, nil)
}

