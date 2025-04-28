
package ism

import sigil "../core"
import "core:fmt"
import "lib:imgui"
import imgui_vk "lib:imgui/imgui_impl_vulkan"
import imgui_glfw "lib:imgui/imgui_impl_glfw"
import vk "vendor:vulkan"

imgui_pool       : vk.DescriptorPool

init_imgui :: proc(swapchain_create_info: ^vk.SwapchainCreateInfoKHR) {
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
    __ensure(
        vk.CreateDescriptorPool(device, &imgui_pool_info, nil, &imgui_pool), 
        msg = "Failed to create descriptor pool for imgui"
    )

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
    style.Colors[imgui.Col.Header]   = imgui.Vec4 { 0.2, 0.2, 0.2, 1 }
    style.Colors[imgui.Col.WindowBg] = imgui.Vec4 { 0, 0, 0, 0.2 }
    style.Colors[imgui.Col.Border]   = imgui.Vec4 { 0, 0, 0, 0 }
}

draw_ui :: proc(cmd: vk.CommandBuffer, img_view: vk.ImageView) {
    imgui_vk.NewFrame()
    imgui_glfw.NewFrame()
    imgui.NewFrame()

    cam := sigil.query(camera)[0]
    imgui.Begin("performance", nil, { .NoTitleBar, .NoBackground, .NoResize, .NoMove, .NoCollapse, .NoMouseInputs })
    {
        //imgui.PushStyleColor(.Text, imgui.GetColorU32(.Header))
        imgui.SetWindowPos(imgui.Vec2 { 0, 0 })
        imgui.TextUnformatted(fmt.caprintf("fps: %.0f", fps))
        imgui.TextUnformatted(fmt.caprintf("ms:  %.3f", ms))
        imgui.TextUnformatted("")
        imgui.TextUnformatted(fmt.caprintf("fwd:   %.3f", cam.forward))
        imgui.TextUnformatted(fmt.caprintf("up:    %.3f", cam.up))
        imgui.TextUnformatted(fmt.caprintf("right: %.3f", cam.right))
        imgui.TextUnformatted("")
        imgui.TextUnformatted(fmt.caprintf("pitch: %.3f", cam.pitch))
        imgui.TextUnformatted(fmt.caprintf("yaw: %.3f", cam.yaw))
        imgui.TextUnformatted("")
        imgui.TextUnformatted(fmt.caprintf("position: %.3f", get_camera_pos()))
        //imgui.PopStyleColor()
    }
    imgui.End()

    imgui.Begin("sigil", nil, { .NoTitleBar, .NoBackground, .NoResize, .NoMove, .NoCollapse, .NoMouseInputs })
    {
        imgui.SetWindowPos(imgui.Vec2 { 0, f32(draw_extent.height - 48) })
        imgui.TextUnformatted(fmt.caprintf("GPU: %s", cstring(&physdevice_props.deviceName[0])))
        imgui.TextUnformatted(fmt.caprintf("sigil %v.%v.%v", sigil.MAJOR_V, sigil.MINOR_V, sigil.PATCH_V))
    }
    imgui.End()
    imgui.Render()

    draw_data := imgui.GetDrawData()
    imgui_vk.RenderDrawData(draw_data, cmd)
}

free_imgui :: proc() {
    vk.DestroyDescriptorPool(device, imgui_pool, nil)
    imgui_vk.DestroyFontsTexture()
    imgui_vk.Shutdown()
    imgui_glfw.Shutdown()
    imgui.DestroyContext()
}

