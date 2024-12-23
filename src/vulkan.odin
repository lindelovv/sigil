//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
package sigil
import "core:dynlib"
import "core:slice"
import "base:runtime"
import vk "vendor:vulkan"
import vma "lib:odin-vma"
import "vendor:glfw"
import "core:os"
import "core:fmt"
import "core:math"
import "core:math/linalg"
import glm "core:math/linalg/glsl"
import "core:mem"
import "lib:assimp"
import "lib:imgui"
import imgui_vk "lib:imgui/imgui_impl_vulkan"
import imgui_glfw "lib:imgui/imgui_impl_glfw"
import "vendor:stb/image"

vulkan :: Module {
    setup = proc() {
        schedule(.INIT, init_vulkan)
        schedule(.TICK, tick_vulkan)
        schedule(.EXIT, terminate_vulkan)
    }
}

//_____________________________
engine_info := vk.ApplicationInfo {
    sType               = .APPLICATION_INFO,
    pApplicationName    = "sigil",
    applicationVersion  = SIGIL_V_U32,
    pEngineName         = "sigil",
    engineVersion       = SIGIL_V_U32,
    apiVersion          = vk.API_VERSION_1_3,
}

validation_layers := []cstring {
    "VK_LAYER_KHRONOS_validation",
    //"VK_LAYER_LUNARG_standard_validation"
}
device_extensions := []cstring {
    vk.KHR_SWAPCHAIN_EXTENSION_NAME,
    vk.KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    vk.KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
}

instance         : vk.Instance
device           : vk.Device
phys_device      : vk.PhysicalDevice
physdevice_props : vk.PhysicalDeviceProperties
vma_allocator    : vma.Allocator
queue_family     : u32
queue            : vk.Queue
surface          : vk.SurfaceKHR
swapchain        : vk.SwapchainKHR
swapchain_images : [dynamic]vk.Image
swap_views       : [dynamic]vk.ImageView
swap_extent      : vk.Extent2D
resize_window    : bool = true
frames           : [2]Frame
current_frame    : u32
desc_pool        : vk.DescriptorPool
imgui_pool       : vk.DescriptorPool
comp_pipeline    : vk.Pipeline
comp_layout      : vk.PipelineLayout
draw_img         : AllocatedImage
draw_descriptor  : vk.DescriptorSet
draw_extent      : vk.Extent2D
depth_img        : AllocatedImage
immediate        : ImmediateSubmit
sampler          : vk.Sampler
meshes           : [dynamic]Mesh
error_image      : AllocatedImage
scene_data       : SceneData
gpu_scene_data   : GPU_SceneData
scene_allocation : AllocatedBuffer
pbr              : PBR_Material

//_____________________________
Frame :: struct {
    pool       : vk.CommandPool,
    cmd        : vk.CommandBuffer,
    fence      : vk.Fence,
    swap_sem   : vk.Semaphore,
    render_sem : vk.Semaphore,
    descriptor : DescriptorData,
}

//_____________________________
ImmediateSubmit :: struct {
    fence: vk.Fence,
    cmd  : vk.CommandBuffer,
    pool : vk.CommandPool,
}

//_____________________________
AllocatedBuffer :: struct {
    handle     : vk.Buffer,
    allocation : vma.Allocation,
    info       : vma.AllocationInfo,
}

AllocatedImage :: struct {
    handle     : vk.Image,
    view       : vk.ImageView,
    allocation : vma.Allocation,
    extent     : vk.Extent3D,
    format     : vk.Format,
}

AllocatedHandle :: union {
    AllocatedBuffer, 
    AllocatedImage,
}

//_____________________________
Vertex :: struct #align(16) {
    position : glm.vec3,
    uv_x     : f32,
    normal   : glm.vec3,
    uv_y     : f32,
    color    : glm.vec4,
}

//_____________________________
Bounds :: struct {
    origin : glm.vec3,
    extent : glm.vec3,
    radius : f32,
}

//_____________________________
Mesh :: struct {
    id          : u32,
    surfaces    : [dynamic]RenderData,
    buffers     : GPUMeshBuffer,
}
RenderData :: struct {
    count       : u32,
    first       : u32,
    idx_buffer  : vk.Buffer,
    material    : ^PBR_Material,
    transform   : glm.mat4,
    address     : vk.DeviceAddress,
}

//_____________________________
GPUMeshBuffer :: struct {
    index   : AllocatedBuffer,
    vertex  : AllocatedBuffer,
    address : vk.DeviceAddress
}

//_____________________________
GPU_PushConstants :: struct {
    vertex_buffer : vk.DeviceAddress,
    transform     : glm.mat4,
    time          : f32
}

//_____________________________
DescriptorData :: struct {
    set         : vk.DescriptorSet,
    bindings    : [dynamic]vk.DescriptorSetLayoutBinding,
    buffer_info : [dynamic]vk.DescriptorBufferInfo,
    image_info  : [dynamic]vk.DescriptorImageInfo,
}

DescriptorImageInfo :: struct {
    view    : vk.ImageView,
    sampler : vk.Sampler,
    layout  : vk.ImageLayout,
}
DescriptorBufferInfo :: struct {
    buffer  : AllocatedBuffer,
    size    : vk.DeviceSize,
    offset  : vk.DeviceSize,
}
DescriptorInfo :: union {
    DescriptorImageInfo,
    DescriptorBufferInfo
}

//_____________________________
ComputePushConstant :: struct {
    d1: glm.vec4,
    d2: glm.vec4,
    d3: glm.vec4,
    d4: glm.vec4
}
sky := ComputePushConstant { { .4, .4, .8, 1 }, { .6, .4, .4, 1 }, {}, {} }

//_____________________________
SceneData :: struct {
    set_layout        : vk.DescriptorSetLayout,
    set               : vk.DescriptorSet,
    pool              : vk.DescriptorPool,
    pipeline          : vk.Pipeline,
    pipeline_layout   : vk.PipelineLayout,
}
GPU_SceneData :: struct {
    view          : glm.mat4,
    proj          : glm.mat4,
    ambient_color : glm.vec4,
    sun_direction : glm.vec4,
    sun_color     : glm.vec4,
}

//_____________________________
dbg_messenger : vk.DebugUtilsMessengerEXT
dbg_callback  : vk.ProcDebugUtilsMessengerCallbackEXT : proc "cdecl" (
    msg_severity:       vk.DebugUtilsMessageSeverityFlagsEXT,
    msg_types:          vk.DebugUtilsMessageTypeFlagsEXT,
    p_callback_data:    ^vk.DebugUtilsMessengerCallbackDataEXT, 
    p_user_data:        rawptr
) -> b32 {
    context = runtime.default_context()
    when ODIN_DEBUG {
        fmt.printf("__%s: %s\n", msg_severity, p_callback_data.pMessage)
    }
    return false
}

//_____________________________
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
    __ensure(vk.CreateInstance(&create_info, nil, &instance), 
        "VK: Instance Creation failed")
    vk.load_proc_addresses(instance)

    //_____________________________
    // Debug Messenger
    when ODIN_DEBUG {
        dbg_create_info: vk.DebugUtilsMessengerCreateInfoEXT = {
            sType           = .DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            messageSeverity = { .WARNING, .ERROR /*, .VERBOSE, .INFO */ },
            messageType     = { .GENERAL, .VALIDATION, .PERFORMANCE },
            pfnUserCallback = dbg_callback
        }
        __ensure(vk.CreateDebugUtilsMessengerEXT(instance, &dbg_create_info, nil, &dbg_messenger), 
            "VK: Debug Utils Messenger Creation Failed")
    }

    //_____________________________
    // Surface
    __ensure(glfw.CreateWindowSurface(instance, window, nil, &surface), 
        "Failed to create window surface")

    //_____________________________
    // Physical Device
    phys_count: u32
    __ensure(vk.EnumeratePhysicalDevices(instance, &phys_count, nil), 
        "VK: EnumeratePhysicalDevices Error")
    phys_device_list := make([dynamic]vk.PhysicalDevice, phys_count)
    __ensure(vk.EnumeratePhysicalDevices(instance, &phys_count, raw_data(phys_device_list)), 
        "VK: EnumeratePhysicalDevices Error")
    phys_device = phys_device_list[0]

    //_____________________________
    // Device
    queue_priorities: f32 = 1.0
    queue_info := vk.DeviceQueueCreateInfo {
        sType            = .DEVICE_QUEUE_CREATE_INFO,
        queueFamilyIndex = queue_family,
        queueCount       = 1,
        pQueuePriorities = &queue_priorities
    }

    features_vk_1_2 := vk.PhysicalDeviceVulkan12Features {
        sType               = .PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        bufferDeviceAddress = true,
        descriptorIndexing  = true,
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
        ppEnabledExtensionNames = raw_data(device_extensions)
    }
    vk.CreateDevice(phys_device, &device_create_info, nil, &device)
    vk.GetDeviceQueue(device, queue_family, 0, &queue)

    //_____________________________
    // Allocator [VMA]
    vk_fns := vma.create_vulkan_functions()
    vma_info := vma.AllocatorCreateInfo {
        vulkanApiVersion = vk.API_VERSION_1_3,
        physicalDevice   = phys_device,
        device           = device,
        instance         = instance,
        pVulkanFunctions = &vk_fns,
        flags            = { .BUFFER_DEVICE_ADDRESS }
    }
    __ensure(vma.CreateAllocator(&vma_info, &vma_allocator),
        "Failed to create memory allocator")

    //_____________________________
    // Swapchain
    capabilities: vk.SurfaceCapabilitiesKHR
    vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &capabilities)
    width, height := glfw.GetFramebufferSize(window)
    swap_extent = vk.Extent2D { u32(width), u32(height) }
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
        imageExtent      = swap_extent,
        minImageCount    = capabilities.minImageCount + 1,
    }
    __ensure(vk.CreateSwapchainKHR(device, &swapchain_create_info, nil, &swapchain),
        "Failed to create swapchain")

    //_____________________________
    // Images
    swapchain_img_count: u32
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, nil)
    swapchain_images = make([dynamic]vk.Image, swapchain_img_count)
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, raw_data(swapchain_images))

    for i: u32 = 0; i < swapchain_img_count; i += 1 {
        img_view: vk.ImageView
        img_view_create_info := vk.ImageViewCreateInfo {
            sType            = .IMAGE_VIEW_CREATE_INFO,
            viewType         = .D2,
            format           = .B8G8R8A8_UNORM,
            image            = swapchain_images[i],
            subresourceRange = {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            }
        }
        vk.CreateImageView(device, &img_view_create_info, nil, &img_view)
        append(&swap_views, img_view)
    }

    //_____________________________
    // Draw & depth image
    //
    // wrong draw format used rn ?
    draw_extent = swap_extent
    extent := vk.Extent3D { swap_extent.width, swap_extent.height, 1 }
    draw_img = create_image(.B8G8R8A8_UNORM, { .TRANSFER_SRC, .TRANSFER_DST, .STORAGE, .COLOR_ATTACHMENT }, extent)

    depth_img = create_image(.D32_SFLOAT, { .DEPTH_STENCIL_ATTACHMENT }, extent)

    //_____________________________
    // Frames
    for &frame in frames {
        pool_create_info := vk.CommandPoolCreateInfo {
            sType            = .COMMAND_POOL_CREATE_INFO,
            queueFamilyIndex = queue_family,
            flags            = { .RESET_COMMAND_BUFFER },
        }
        __ensure(vk.CreateCommandPool(device, &pool_create_info, nil, &frame.pool),
            "Failed to create command pool")

        cmd_create_info := vk.CommandBufferAllocateInfo {
            sType              = .COMMAND_BUFFER_ALLOCATE_INFO,
            commandPool        = frame.pool,
            level              = .PRIMARY,
            commandBufferCount = 1
        }
        __ensure(vk.AllocateCommandBuffers(device, &cmd_create_info, &frame.cmd),
            "Failed to create command buffer")

        fence_create_info := vk.FenceCreateInfo {
            sType = .FENCE_CREATE_INFO,
            flags = { .SIGNALED }
        }
        __ensure(vk.CreateFence(device, &fence_create_info, nil, &frame.fence),
            "Failed to create fence")

        semaphore_create_info := vk.SemaphoreCreateInfo {
            sType = .SEMAPHORE_CREATE_INFO
        }
        __ensure(vk.CreateSemaphore(device, &semaphore_create_info, nil, &frame.swap_sem),
            "Failed to create swap semaphore")
        __ensure(vk.CreateSemaphore(device, &semaphore_create_info, nil, &frame.render_sem),
            "Failed to create render semaphore")

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
        __ensure(vk.CreateDescriptorPool(device, &desc_pool_info, nil, &desc_pool),
            "Failed to create frame descriptor pool")

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
        vk.CreateDescriptorSetLayout(device, &set_layout_info, nil, &layout)

        //_____________________________
        // Allocate Descriptor Set
        allocate_info := vk.DescriptorSetAllocateInfo {
            sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
            descriptorPool     = desc_pool,
            descriptorSetCount = 1,
            pSetLayouts        = &layout
        }
        vk.AllocateDescriptorSets(device, &allocate_info, &frame.descriptor.set)

        //_____________________________
        // Update Descriptor Data
        gpu_scene_data = GPU_SceneData {
            view          = get_view(main_camera),
            proj          = linalg.matrix4_perspective(
                                glm.radians_f32(main_camera.fov), 
                                f32(swap_extent.width)/f32(swap_extent.height), 
                                main_camera.near, main_camera.far, 
                            ),
            sun_color     = { 0, 1, 1,  1 },
            sun_direction = { 0, 0, 0, 0 },
            ambient_color = { 1, 1, 1,  1 },
        }
        scene_allocation = create_buffer(size_of(GPU_SceneData), { .UNIFORM_BUFFER }, .CPU_TO_GPU)
        scene_unifrom_data := cast(^GPU_SceneData)scene_allocation.info.pMappedData
        scene_unifrom_data = &gpu_scene_data

        scene_desc := DescriptorData { set = frame.descriptor.set }
        scene_writes := []vk.WriteDescriptorSet {
            write_descriptor(&scene_desc, 0, .UNIFORM_BUFFER, DescriptorBufferInfo { scene_allocation, size_of(GPU_SceneData), 0 }),
        }
        vk.UpdateDescriptorSets(device, u32(len(scene_writes)), raw_data(scene_writes), 0, nil)
    }

    //_____________________________
    // Immediate Submit

    pool_create_info := vk.CommandPoolCreateInfo {
        sType            = .COMMAND_POOL_CREATE_INFO,
        queueFamilyIndex = queue_family,
        flags            = { .RESET_COMMAND_BUFFER },
    }
    __ensure(vk.CreateCommandPool(device, &pool_create_info, nil, &immediate.pool),
        "Failed to create command pool")

    cmd_create_info := vk.CommandBufferAllocateInfo {
        sType              = .COMMAND_BUFFER_ALLOCATE_INFO,
        commandPool        = immediate.pool,
        level              = .PRIMARY,
        commandBufferCount = 1
    }
    __ensure(vk.AllocateCommandBuffers(device, &cmd_create_info, &immediate.cmd), 
        "Failed to create command buffer")

    fence_create_info := vk.FenceCreateInfo {
        sType = .FENCE_CREATE_INFO,
        flags = { .SIGNALED }
    }
    __ensure(vk.CreateFence(device, &fence_create_info, nil, &immediate.fence), 
        "Failed to create fence")

    debug_label := vk.DebugUtilsLabelEXT {
        sType      = .DEBUG_UTILS_LABEL_EXT,
        pLabelName = "init",
        color      = { 0, 255, 0, 255 },
    }
    vk.CmdBeginDebugUtilsLabelEXT(immediate.cmd, &debug_label)

    //_____________________________
    // Descriptor Pool
    sizes := [dynamic]vk.DescriptorPoolSize {
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
    __ensure(vk.CreateDescriptorPool(device, &desc_pool_info, nil, &desc_pool), 
        "Failed to create descriptor pool")

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
    vk.CreateDescriptorSetLayout(device, &set_layout_info, nil, &layout)

    //_____________________________
    // Descriptor Set
    allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &layout
    }
    vk.AllocateDescriptorSets(device, &allocate_info, &draw_descriptor)

    draw_write := vk.WriteDescriptorSet {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = draw_descriptor,
        dstBinding      = 0,
        descriptorCount = 1,
        descriptorType  = .STORAGE_IMAGE,
        pImageInfo      = &img_info,
    }
    vk.UpdateDescriptorSets(device, 1, &draw_write, 0, nil)

    //_____________________________
    // Sampler
    sampler_info := vk.SamplerCreateInfo {
        sType     = .SAMPLER_CREATE_INFO,
        magFilter = .LINEAR,
        minFilter = .LINEAR,
    }
    __ensure(vk.CreateSampler(device, &sampler_info, nil, &sampler), 
        "Failed to create sampler")

    //_____________________________
    // Compute Pipeline Layout
    compute_range := vk.PushConstantRange {
        stageFlags = { .COMPUTE },
        offset     = 0,
        size       = size_of(ComputePushConstant)
    }
    layout_info := vk.PipelineLayoutCreateInfo {
        sType                  = .PIPELINE_LAYOUT_CREATE_INFO,
        setLayoutCount         = 1,
        pSetLayouts            = &layout,
        pushConstantRangeCount = 1,
        pPushConstantRanges    = &compute_range,
    }
    __ensure(vk.CreatePipelineLayout(device, &layout_info, nil, &comp_layout), 
        "Failed to create pipeline layout")

    //_____________________________
    // Compute Pipeline
    comp_shader := create_shader_module("res/shaders/gradient.comp.spv")
    defer vk.DestroyShaderModule(device, comp_shader, nil)

    comp_stage_info := vk.PipelineShaderStageCreateInfo {
        sType  = .PIPELINE_SHADER_STAGE_CREATE_INFO,
        stage  = { .COMPUTE },
        module = comp_shader,
        pName  = "main"
    }
    comp_pipe_info := vk.ComputePipelineCreateInfo {
        sType  = .COMPUTE_PIPELINE_CREATE_INFO,
        stage  = comp_stage_info,
        layout = comp_layout
    }
    __ensure(vk.CreateComputePipelines(device, 0, 1, &comp_pipe_info, nil, &comp_pipeline), 
        "Failed to create Compute Pipeline")

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
    error_image := create_image_from_buffer(&pixels, { 16, 16, 1 }, .R8G8B8A8_UNORM, { .SAMPLED })

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
    __ensure(vk.CreateDescriptorSetLayout(device, &scene_data_layout_info, nil, &scene_data.set_layout), 
        "Failed to create descriptor set")
    scene_data_allocate_info := vk.DescriptorSetAllocateInfo {
        sType              = .DESCRIPTOR_SET_ALLOCATE_INFO,
        descriptorPool     = desc_pool,
        descriptorSetCount = 1,
        pSetLayouts        = &scene_data.set_layout,
    }
    __ensure(vk.AllocateDescriptorSets(device, &scene_data_allocate_info, &scene_data.set), 
        "Failed to allocate descriptor set")

    //_____________________________
    // PBR Material
    pbr_declare() // TODO: make switching easier

    //_____________________________
    // Imgui
    imgui_pool_sizes := []vk.DescriptorPoolSize {
        { .SAMPLER,                1000 },
        { .SAMPLED_IMAGE,          1000 },
        { .STORAGE_IMAGE,          1000 },
        { .UNIFORM_TEXEL_BUFFER,   1000 },
        { .STORAGE_TEXEL_BUFFER,   1000 },
        { .UNIFORM_BUFFER,         1000 },
        { .STORAGE_BUFFER,         1000 },
        { .UNIFORM_BUFFER_DYNAMIC, 1000 },
        { .STORAGE_BUFFER_DYNAMIC, 1000 },
        { .INPUT_ATTACHMENT,       1000 },
    }
    imgui_pool_info := vk.DescriptorPoolCreateInfo {
        sType         = .DESCRIPTOR_POOL_CREATE_INFO,
        flags         = { .FREE_DESCRIPTOR_SET },
        maxSets       = 1000,
        poolSizeCount = u32(len(imgui_pool_sizes)),
        pPoolSizes    = raw_data(imgui_pool_sizes),
    }
    __ensure(vk.CreateDescriptorPool(device, &imgui_pool_info, nil, &imgui_pool), 
        "Failed to create descriptor pool for imgui")

    imgui.CHECKVERSION()
    imgui.CreateContext()

    imgui_vk.LoadFunctions(proc "c" (function_name: cstring, user_data: rawptr) -> vk.ProcVoidFunction {
		return vk.GetInstanceProcAddr(auto_cast user_data, function_name)
	}, auto_cast instance)

    imgui_glfw.InitForVulkan(window, true)
    imgui_init_info := imgui_vk.InitInfo {
        Instance            = instance,
        PhysicalDevice      = phys_device,
        Device              = device,
        Queue               = queue,
        DescriptorPool      = imgui_pool,
        MinImageCount       = 4,
        ImageCount          = 4,
        MSAASamples         = ._1,
        UseDynamicRendering = true,
        PipelineRenderingCreateInfo = vk.PipelineRenderingCreateInfo {
            sType                   = .PIPELINE_RENDERING_CREATE_INFO,
            colorAttachmentCount    = 1,
            pColorAttachmentFormats = &swapchain_create_info.imageFormat,
            depthAttachmentFormat   = depth_img.format,
        }
    }
    imgui_vk.Init(&imgui_init_info)

    io := imgui.GetIO()
    io.IniFilename = nil
    io.LogFilename = nil
    io.FontDefault = imgui.FontAtlas_AddFontFromFileTTF(io.Fonts, "res/fonts/NotoSansMono-Regular.ttf", 12)
    imgui.FontAtlas_Build(io.Fonts)
    imgui_vk.CreateFontsTexture()

    style := imgui.GetStyle()
    style.WindowRounding    = 8
    style.FrameRounding     = 8
    style.ScrollbarRounding = 4
    style.Colors[imgui.Col.Text]     = imgui.Vec4 { 0.6, 0.6, 0.6, 1 }
    style.Colors[imgui.Col.WindowBg] = imgui.Vec4 { 0, 0, 0, 0.2 }
    style.Colors[imgui.Col.Border]   = imgui.Vec4 { 0, 0, 0, 0 }

    //_____________________________
    // Upload Mesh
    load_mesh("res/models/DamagedHelmet.gltf")

    rect_data := RenderData {
        first     = 0,
        count     = u32(len(rect_indices)),
    }
    append(&rect_mesh.surfaces, rect_data)
    upload_mesh(rect, rect_indices, &rect_mesh)

    vk.GetPhysicalDeviceProperties(phys_device, &physdevice_props)

    vk.CmdEndDebugUtilsLabelEXT(immediate.cmd)
}

rebuild_swapchain :: proc() {
    defer resize_window = false
    __ensure(vk.QueueWaitIdle(queue), 
        "Waiting for queue failed")

    vk.DestroySwapchainKHR(device, swapchain, nil)

    capabilities: vk.SurfaceCapabilitiesKHR
    vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &capabilities)

    width, height := glfw.GetFramebufferSize(window)
    swap_extent = vk.Extent2D { u32(width), u32(height) }

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
        imageExtent      = swap_extent,
        minImageCount    = capabilities.minImageCount + 1,
    }
    __ensure(vk.CreateSwapchainKHR(device, &swapchain_create_info, nil, &swapchain), 
        "Failed to create swapchain")

    //_____________________________
    // Images
    swapchain_img_count: u32
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, nil)
    swapchain_images = make([dynamic]vk.Image, swapchain_img_count)
    vk.GetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, raw_data(swapchain_images))

    for i: u32 = 0; i < swapchain_img_count; i += 1 {
        img_view: vk.ImageView
        img_view_create_info := vk.ImageViewCreateInfo {
            sType            = .IMAGE_VIEW_CREATE_INFO,
            viewType         = .D2,
            format           = .B8G8R8A8_UNORM,
            image            = swapchain_images[i],
            subresourceRange = {
                aspectMask      = { .COLOR },
                layerCount      = 1,
                levelCount      = 1
            }
        }
        vk.CreateImageView(device, &img_view_create_info, nil, &img_view)
        append(&swap_views, img_view)
    }
}

//_____________________________
create_shader_module :: proc(path: string) -> (shader: vk.ShaderModule) {
    data, result := os.read_entire_file(path)
    __ensure(result, "Failed to open file")
    shader_module_info := vk.ShaderModuleCreateInfo {
        sType    = .SHADER_MODULE_CREATE_INFO,
        codeSize = len(data),
        pCode    = cast(^u32) raw_data(data)
    }
    __ensure(vk.CreateShaderModule(device, &shader_module_info, nil, &shader), 
        "Failed to create shader module")
    return shader
}

//_____________________________
write_descriptor :: proc(
    data: ^DescriptorData, binding: u32, 
    type: vk.DescriptorType, 
    resource: DescriptorInfo
) -> (write: vk.WriteDescriptorSet) {
    write = {
        sType           = .WRITE_DESCRIPTOR_SET,
        dstSet          = data.set,
        dstBinding      = binding,
        descriptorCount = 1,
        descriptorType  = type,
    }
    switch info in resource {
        case DescriptorImageInfo:  {
            img_info := vk.DescriptorImageInfo {
                sampler      = info.sampler,
                imageView    = info.view,
                imageLayout  = info.layout,
            }
            append(&data.image_info, img_info)
            i := len(data.image_info) - 1
            write.pImageInfo = &data.image_info[i]
        }
        case DescriptorBufferInfo: {
            buffer_info := vk.DescriptorBufferInfo {
                buffer = info.buffer.handle,
                offset = info.offset,
                range  = info.size,
            }
            append(&data.buffer_info, buffer_info)
            i := len(data.buffer_info) - 1
            write.pBufferInfo = &data.buffer_info[i]
        }
    }
    return write
}

//_____________________________
upload_mesh :: proc(vertex_buffer: [dynamic]Vertex, index_buffer: [dynamic]u32, mesh: ^Mesh) {
    mesh.id = u32(len(meshes))

    vtx_buffer_size := vk.DeviceSize(len(vertex_buffer) * size_of(Vertex))
    idx_buffer_size := vk.DeviceSize(len(index_buffer)  * size_of(u32))

    mesh.buffers = {
        vertex = create_buffer(vtx_buffer_size, { .STORAGE_BUFFER, .TRANSFER_DST, .SHADER_DEVICE_ADDRESS }, .GPU_ONLY),
        index  = create_buffer(idx_buffer_size, { .INDEX_BUFFER, .TRANSFER_DST }, .GPU_ONLY),
    }
    device_address_info := vk.BufferDeviceAddressInfo {
        sType  = .BUFFER_DEVICE_ADDRESS_INFO,
        buffer = mesh.buffers.vertex.handle,
    }
    mesh.buffers.address = vk.GetBufferDeviceAddress(device, &device_address_info)
    
    for &data in mesh.surfaces {
        data.address    = mesh.buffers.address
        data.idx_buffer = mesh.buffers.index.handle
        data.transform  = glm.mat4(1)
    }

    staging_buffer := create_buffer(vtx_buffer_size + idx_buffer_size, { .TRANSFER_SRC }, .CPU_ONLY)
    defer destroy_buffer(staging_buffer)
    buffer_data := staging_buffer.info.pMappedData
    mem.copy(buffer_data, raw_data(vertex_buffer), int(vtx_buffer_size))
	mem.copy(mem.ptr_offset((^u8)(buffer_data), vtx_buffer_size), raw_data(index_buffer), int(idx_buffer_size))

    __ensure(vk.ResetFences(device, 1, &immediate.fence), 
        "Failed resetting immediate fence")
    __ensure(vk.ResetCommandBuffer(immediate.cmd, {}), 
        "Failed to reset command buffer")

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }
    __ensure(vk.BeginCommandBuffer(immediate.cmd, &cmd_begin_info), 
        "Failed to being immediate command buffer")
    {
        vtx_copy := vk.BufferCopy {
            srcOffset = 0,
            dstOffset = 0,
            size      = vtx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, mesh.buffers.vertex.handle, 1, &vtx_copy)
        idx_copy := vk.BufferCopy {
            srcOffset = vtx_buffer_size,
            dstOffset = 0,
            size      = idx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, mesh.buffers.index.handle, 1, &idx_copy)
    }
    __ensure(vk.EndCommandBuffer(immediate.cmd), 
        "Failed to end immediate command buffer")

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
    __ensure(vk.QueueSubmit2(queue, 1, &submit_info, immediate.fence), 
        "Failed to sumbit queue")
    __ensure(vk.WaitForFences(device, 1, &immediate.fence, true, max(u64)), 
        "Failed waiting for fences")

    append(&meshes, mesh^)
}

//_____________________________
load_mesh :: proc(path: cstring) {
    file := assimp.ImportFile(path, { .Triangulate, .FlipUVs })
    if file.mNumMeshes == 0 do return

    mesh := Mesh { id = u32(len(meshes)) }

    vertex_buffer: [dynamic]Vertex
    index_buffer:  [dynamic]u32
    for m: u32 = 0; m < file.mNumMeshes; m += 1 {
        submesh := file.mMeshes[m]
        data := RenderData {
            material  = &pbr,
            transform = glm.mat4Rotate(glm.vec3(1), glm.radians_f32(90)),
        }

        data.first = u32(len(index_buffer))
        for f: u32 = 0; f < submesh.mNumFaces; f += 1 {
            face := submesh.mFaces[f]
            assert(face.mNumIndices == 3, "Face does not have 3 indices")
            append(&index_buffer, face.mIndices[0])
            append(&index_buffer, face.mIndices[1])
            append(&index_buffer, face.mIndices[2])
        }
        data.count = u32(len(index_buffer))

        for v: u32 = 0; v < submesh.mNumVertices; v += 1 {
            vert := Vertex {
                position = {
                    f32(submesh.mVertices[v].x),
                    f32(submesh.mVertices[v].y),
                    f32(submesh.mVertices[v].z)
                },
                uv_x = f32(submesh.mTextureCoords[0][v].x),
                normal = {
                    f32(submesh.mNormals[v].x), 
                    f32(submesh.mNormals[v].y), 
                    f32(submesh.mNormals[v].z)
                },
                uv_y = f32(submesh.mTextureCoords[0][v].y),
                color = { 1, 1, 1, 1 }
            }
            append(&vertex_buffer, vert)
        }
        append(&mesh.surfaces, data)
    }

    //________________
    // upload mesh
    vtx_buffer_size := vk.DeviceSize(len(vertex_buffer) * size_of(Vertex))
    idx_buffer_size := vk.DeviceSize(len(index_buffer) * size_of(u32))

    mesh.buffers = {
        vertex = create_buffer(vtx_buffer_size, { .STORAGE_BUFFER, .TRANSFER_DST, .SHADER_DEVICE_ADDRESS }, .GPU_ONLY),
        index  = create_buffer(idx_buffer_size, { .INDEX_BUFFER, .TRANSFER_DST }, .GPU_ONLY),
    }

    device_address_info := vk.BufferDeviceAddressInfo {
        sType  = .BUFFER_DEVICE_ADDRESS_INFO,
        buffer = mesh.buffers.vertex.handle,
    }
    mesh.buffers.address = vk.GetBufferDeviceAddress(device, &device_address_info)
    
    for &data in mesh.surfaces {
        data.address = mesh.buffers.address
        data.idx_buffer = mesh.buffers.index.handle
    }

    staging_buffer := create_buffer(vtx_buffer_size + idx_buffer_size, { .TRANSFER_SRC }, .CPU_ONLY)
    defer destroy_buffer(staging_buffer)
    buffer_data := staging_buffer.info.pMappedData
    mem.copy(buffer_data, raw_data(vertex_buffer), int(vtx_buffer_size))
	mem.copy(mem.ptr_offset((^u8)(buffer_data), vtx_buffer_size), raw_data(index_buffer), int(idx_buffer_size))

    __ensure(vk.ResetFences(device, 1, &immediate.fence), 
        "Failed resetting immediate fence")
    __ensure(vk.ResetCommandBuffer(immediate.cmd, {}), 
        "Failed to reset command buffer")

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }
    __ensure(vk.BeginCommandBuffer(immediate.cmd, &cmd_begin_info), "Failed to being immediate command buffer")
    {
        vtx_copy := vk.BufferCopy {
            srcOffset = 0,
            dstOffset = 0,
            size      = vtx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, mesh.buffers.vertex.handle, 1, &vtx_copy)
        idx_copy := vk.BufferCopy {
            srcOffset = vtx_buffer_size,
            dstOffset = 0,
            size      = idx_buffer_size,
        }
        vk.CmdCopyBuffer(immediate.cmd, staging_buffer.handle, mesh.buffers.index.handle, 1, &idx_copy)
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
    __ensure(vk.QueueSubmit2(queue, 1, &submit_info, immediate.fence), "Failed to sumbit queue")
    __ensure(vk.WaitForFences(device, 1, &immediate.fence, true, max(u64)), "Failed waiting for fences")

    append(&meshes, mesh)
}

//_____________________________
create_buffer :: proc(
    buffer_size: vk.DeviceSize, 
    buffer_usage: vk.BufferUsageFlags, 
    mem_usage: vma.MemoryUsage
) -> (buffer: AllocatedBuffer) {
    buffer_info := vk.BufferCreateInfo {
        sType = .BUFFER_CREATE_INFO,
        size  = buffer_size,
        usage = buffer_usage,
    }
    alloc_info := vma.AllocationCreateInfo {
        flags = { .MAPPED },
        usage = mem_usage,
    }
    __ensure(vma.CreateBuffer(vma_allocator, &buffer_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info), 
        "Failed to create buffer")
    return buffer
}

//_____________________________
destroy_buffer :: proc(buffer: AllocatedBuffer) {
    vma.DestroyBuffer(vma_allocator, buffer.handle, buffer.allocation)
}

//_____________________________
load_image :: proc(format: vk.Format, usage: vk.ImageUsageFlags, path: cstring) -> AllocatedImage {
    width, height: i32
    pixels := image.load(path, &width, &height, nil, 4 /* STBI_rgb_alpha */)
    if pixels == nil {
        __log("Failed to load image, path: ", path)
        return error_image
    }
    return create_image_from_buffer(pixels, { u32(width), u32(height), 1 }, .R8G8B8A8_UNORM, { .SAMPLED })
}

//_____________________________
create_image :: proc(format: vk.Format, flags: vk.ImageUsageFlags, extent: vk.Extent3D) -> AllocatedImage {
    alloc_img := AllocatedImage { extent = extent, format = format }

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
    __ensure(vma.CreateImage(vma_allocator, &img_info, &alloc_info, &alloc_img.handle, &alloc_img.allocation, nil), 
        "Failed to create image")

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
    __ensure(vk.CreateImageView(device, &view_info, nil, &alloc_img.view), "Failed to create image view")

    return alloc_img
}

//_____________________________
create_image_from_buffer :: proc(data: rawptr, extent: vk.Extent3D, format: vk.Format, usage: vk.ImageUsageFlags) -> AllocatedImage {
    size: vk.DeviceSize = vk.DeviceSize(extent.depth * extent.width * extent.height * 4)
    upload_buffer := create_buffer(size, { .TRANSFER_SRC }, .CPU_TO_GPU)
    defer destroy_buffer(upload_buffer)
    mem.copy(upload_buffer.info.pMappedData, data, int(size))

    img := create_image(format, usage + { .TRANSFER_DST, .TRANSFER_SRC }, extent)

    __ensure(vk.ResetFences(device, 1, &immediate.fence), 
        "Failed resetting immediate fence")
    __ensure(vk.ResetCommandBuffer(immediate.cmd, {}), 
        "Failed to reset command buffer")

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT }
    }
    __ensure(vk.BeginCommandBuffer(immediate.cmd, &cmd_begin_info), 
        "Failed to being immediate command buffer")
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
    __ensure(vk.EndCommandBuffer(immediate.cmd), 
        "Failed to end immediate command buffer")
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
    __ensure(vk.QueueSubmit2(queue, 1, &submit_info, immediate.fence), 
        "Failed to sumbit queue")
    __ensure(vk.WaitForFences(device, 1, &immediate.fence, true, max(u64)), 
        "Failed waiting for fences")

    return img
}

//_____________________________
transition_img :: proc(cmd: vk.CommandBuffer, img: vk.Image, from: vk.ImageLayout, to: vk.ImageLayout) {
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

//_____________________________
blit_imgs :: proc(cmd: vk.CommandBuffer, src: vk.Image, dst: vk.Image, src_extent: vk.Extent2D, dst_extent: vk.Extent2D) {
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

//_____________________________
tick_vulkan :: proc() {
    if resize_window do rebuild_swapchain()

    frame := frames[current_frame]
    err := vk.WaitForFences(device, 1, &frame.fence, true, max(u64)); if err == .ERROR_DEVICE_LOST {
        __log("Wait for fences failed. Device Lost!")
        glfw.SetWindowShouldClose(window, true)
    }
    __ensure(vk.WaitForFences(device, 1, &frame.fence, true, max(u64)), 
        "WaitForFences failed")

    img_index: u32
    result := vk.AcquireNextImageKHR(device, swapchain, max(u64), frame.swap_sem, {}, &img_index)
    ; if result == .ERROR_OUT_OF_DATE_KHR || result == .SUBOPTIMAL_KHR { resize_window = true }
    swap_img := swapchain_images[img_index]

    __ensure(vk.ResetFences(device, 1, &frame.fence), 
        "Reset fence failed")
    vk.ResetCommandBuffer(frame.cmd, {})

    cmd_begin_info := vk.CommandBufferBeginInfo {
        sType = .COMMAND_BUFFER_BEGIN_INFO,
        flags = { .ONE_TIME_SUBMIT },
    }
    draw_extent.width  = math.min(swap_extent.width, draw_img.extent.width);
    draw_extent.height = math.min(swap_extent.width, draw_img.extent.height);
    __ensure(vk.BeginCommandBuffer(frame.cmd, &cmd_begin_info), "Begin cmd failed")
    {
        transition_img(frame.cmd, draw_img.handle, .UNDEFINED, .GENERAL)

        //________________
        // draw backgrund
        vk.CmdBindPipeline(frame.cmd, .COMPUTE, comp_pipeline)
        vk.CmdBindDescriptorSets(frame.cmd, .COMPUTE, comp_layout, 0, 1, &draw_descriptor, 0, nil)
        vk.CmdPushConstants(frame.cmd, comp_layout, { .COMPUTE }, 0, size_of(ComputePushConstant), &sky)
        vk.CmdDispatch(frame.cmd, u32(math.ceil(f32(draw_extent.width) / 16.0)), u32(math.ceil(f32(draw_extent.height) / 16.0)), 1)

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

            @(require_results)
            matrix4_perspective_zero_to_one_f32 :: proc "contextless" (
                fovy, aspect, near, far: f32, 
                flip_z_axis := true
            ) -> (m: linalg.Matrix4f32) #no_bounds_check {
                tan_half_fovy := math.tan(0.5 * fovy)
                m[0, 0] = 1 / (aspect*tan_half_fovy)
                m[1, 1] = 1 / (tan_half_fovy)
                m[2, 2] = far / (far - near)
                m[3, 2] = +1
                m[2, 3] = (far*near) / (far - near)
                m[3, 3] = 1
            
                if flip_z_axis {
                    m[2] = -m[2]
                }
            
                return
            }
            view := get_view(main_camera)
            projection := glm.mat4PerspectiveInfinite(
                glm.radians(main_camera.fov),
                f32(swap_extent.width) / f32(swap_extent.height),
                main_camera.far,
            )
            //projection := matrix4_perspective_zero_to_one_f32(
            //    glm.radians(main_camera.fov),
            //    f32(swap_extent.width) / f32(swap_extent.height),
            //    main_camera.far,
            //    main_camera.near,
            //)

            gpu_scene_data.view = view
            gpu_scene_data.proj = projection
            mem.copy(scene_allocation.info.pMappedData, &gpu_scene_data, size_of(GPU_SceneData))

            scene_data_desc := DescriptorData { set = frame.descriptor.set }
            scene_data_writes := []vk.WriteDescriptorSet {
                write_descriptor(&scene_data_desc, 0, .UNIFORM_BUFFER, DescriptorBufferInfo { scene_allocation, size_of(GPU_SceneData), 0 }),
            }
            vk.UpdateDescriptorSets(device, u32(len(scene_data_writes)), raw_data(scene_data_writes), 0, nil)

            vk.CmdBindPipeline(frame.cmd, .GRAPHICS, pbr.pipeline)
            vk.CmdBindDescriptorSets(frame.cmd, .GRAPHICS, pbr.pipeline_layout, 0, 1, &frame.descriptor.set, 0, nil)
            vk.CmdBindDescriptorSets(frame.cmd, .GRAPHICS, pbr.pipeline_layout, 1, 1, &pbr.set,              0, nil)

            push_const := GPU_PushConstants { time = time  }
            camera_matrix := projection * view

            for mesh in meshes {
                for data in mesh.surfaces {
                    push_const.vertex_buffer = data.address
                    push_const.transform = data.transform
                    //push_const.transform[3][3] = 1
                    vk.CmdBindIndexBuffer(frame.cmd, data.idx_buffer, 0, .UINT32)
                    vk.CmdPushConstants(frame.cmd, pbr.pipeline_layout, { .VERTEX, .FRAGMENT }, 0, size_of(GPU_PushConstants), &push_const)
                    vk.CmdDrawIndexed(frame.cmd, data.count, 1, data.first, 0, 0)
                }
            }
            draw_ui(frame.cmd, swap_views[current_frame])
        }
        vk.CmdEndRendering(frame.cmd)

        //________________
        // prepare for present
        transition_img(frame.cmd, draw_img.handle, .COLOR_ATTACHMENT_OPTIMAL, .TRANSFER_SRC_OPTIMAL)
        transition_img(frame.cmd, swap_img, .UNDEFINED, .TRANSFER_DST_OPTIMAL)

        blit_imgs(frame.cmd, draw_img.handle, swap_img, draw_extent, swap_extent)

        transition_img(frame.cmd, swap_img, .TRANSFER_DST_OPTIMAL, .PRESENT_SRC_KHR)
    }
    __ensure(vk.EndCommandBuffer(frame.cmd), 
        "End cmd failed")

    cmd_info := vk.CommandBufferSubmitInfo {
        sType         = .COMMAND_BUFFER_SUBMIT_INFO,
        commandBuffer = frame.cmd,
        deviceMask    = 0,
    }
    wait_info := vk.SemaphoreSubmitInfo {
        sType       = .SEMAPHORE_SUBMIT_INFO,
        semaphore   = frame.swap_sem,
        value       = 1,
        stageMask   = { .COLOR_ATTACHMENT_OUTPUT },
        deviceIndex = 0,
    }
    signal_info := vk.SemaphoreSubmitInfo {
        sType       = .SEMAPHORE_SUBMIT_INFO,
        semaphore   = frame.render_sem,
        value       = 1,
        stageMask   = { .ALL_GRAPHICS },
        deviceIndex = 0,
    }

    submit_info := vk.SubmitInfo2 {
        sType                    = .SUBMIT_INFO_2,
        waitSemaphoreInfoCount   = 1,
        pWaitSemaphoreInfos      = &wait_info,
        commandBufferInfoCount   = 1,
        pCommandBufferInfos      = &cmd_info,
        signalSemaphoreInfoCount = 1,
        pSignalSemaphoreInfos    = &signal_info,
    }
    __ensure(vk.QueueSubmit2(queue, 1, &submit_info, frame.fence), 
        "Queue submit failed")

    present_info := vk.PresentInfoKHR {
        sType              = .PRESENT_INFO_KHR,
        waitSemaphoreCount = 1,
        pWaitSemaphores    = &frame.render_sem,
        pImageIndices      = &img_index,
        swapchainCount     = 1,
        pSwapchains        = &swapchain,
    }
    res := vk.QueuePresentKHR(queue, &present_info); if res == .ERROR_OUT_OF_DATE_KHR || res == .SUBOPTIMAL_KHR {
        resize_window = true
    }
    current_frame = (current_frame >= (len(frames) - 1)) ? 0 : current_frame + 1
}

draw_ui :: proc(cmd: vk.CommandBuffer, img_view: vk.ImageView) {
    imgui_vk.NewFrame()
    imgui_glfw.NewFrame()
    imgui.NewFrame()

    imgui.Begin("performance", nil, { .NoTitleBar, .NoBackground, .NoResize, .NoMove, .NoCollapse, .NoMouseInputs })
    {
        imgui.SetWindowPos(imgui.Vec2 { 0, 0 })
        imgui.TextUnformatted(fmt.caprintf("fps: %.0f", fps))
        imgui.TextUnformatted(fmt.caprintf("ms:  %.3f", ms))
    }
    imgui.End()

    imgui.Begin("sigil", nil, { .NoTitleBar, .NoBackground, .NoResize, .NoMove, .NoCollapse, .NoMouseInputs })
    {
        imgui.SetWindowPos(imgui.Vec2 { 0, f32(draw_extent.height - 50) })
        imgui.TextUnformatted(fmt.caprintf("GPU: %s", cstring(&physdevice_props.deviceName[0])))
        imgui.TextUnformatted(fmt.caprintf("sigil %s", SIGIL_V_STR))
    }
    imgui.End()
    imgui.Render()

    draw_data := imgui.GetDrawData()
    imgui_vk.RenderDrawData(draw_data, cmd)
}

terminate_vulkan :: proc() {
    __ensure(vk.DeviceWaitIdle(device))

    vk.DestroyPipelineLayout(device, comp_layout, nil)
    vk.DestroyPipeline(device, comp_pipeline, nil)

    vk.DestroySwapchainKHR(device, swapchain, nil)
    for view in swap_views do vk.DestroyImageView(device, view, nil)
    for img in swapchain_images do vk.DestroyImage(device, img, nil)

    // destroy the rest

    vk.DestroyDescriptorPool(device, imgui_pool, nil)
    imgui_vk.DestroyFontsTexture()
    imgui_vk.Shutdown()
    imgui_glfw.Shutdown()
    imgui.DestroyContext()

    for frame in frames {
        vk.DestroyCommandPool(device, frame.pool, nil)
        vk.DestroyFence(device, frame.fence, nil)
        vk.DestroySemaphore(device, frame.swap_sem, nil)
        vk.DestroySemaphore(device, frame.render_sem, nil)
    }
    vk.DestroyCommandPool(device, immediate.pool, nil)
    vk.DestroyFence(device, immediate.fence, nil)
    vk.DestroySurfaceKHR(instance, surface, nil)
    vk.DestroyDevice(device, nil)
    vk.DestroyDebugUtilsMessengerEXT(instance, dbg_messenger, nil)
    vk.DestroyInstance(instance, nil)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
