#include "vulkan.hh"

namespace sigil::renderer::ui {

    //void draw(vk::CommandBuffer cmd, vk::ImageView img_view, vk::Extent2D extent) {

    //    ImGui_ImplVulkan_NewFrame();
    //    ImGui_ImplGlfw_NewFrame();
    //    ImGui::NewFrame();

    //    ImGui::Begin("Sigil", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
    //                                  | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
    //                                  | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
    //    {
    //        ImGui::TextUnformatted(fmt::format("GPU: {}", phys_device.getProperties().deviceName.data()).c_str());
    //        ImGui::TextUnformatted(fmt::format("sigil   {}", sigil::version::as_string).c_str());
    //        ImGui::SetWindowPos(ImVec2(0, swapchain.extent.height - ImGui::GetWindowSize().y));
    //    }
    //    ImGui::End();

    //    ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar   //| ImGuiWindowFlags_NoBackground
    //                                  | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
    //                                  | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
    //    {
    //        ImGui::TextUnformatted(
    //            fmt::format(" Camera position:\n\tx: {:.3f}\n\ty: {:.3f}\n\tz: {:.3f}",
    //            camera.transform.position.x, camera.transform.position.y, camera.transform.position.z).c_str()
    //        );
    //        ImGui::TextUnformatted(
    //            fmt::format(" Yaw:   {:.2f}\n Pitch: {:.2f}",
    //            camera.yaw, camera.pitch).c_str()
    //        );
    //        ImGui::TextUnformatted(
    //            fmt::format(" Mouse position:\n\tx: {:.0f}\n\ty: {:.0f}",
    //            input::mouse_position.x, input::mouse_position.y ).c_str()
    //        );
    //        ImGui::TextUnformatted(
    //            fmt::format(" Mouse offset:\n\tx: {:.0f}\n\ty: {:.0f}",
    //            input::get_mouse_movement().x, input::get_mouse_movement().y).c_str()
    //        );

    //        ImGui::SetWindowSize(ImVec2(108, 174));
    //        ImGui::SetWindowPos(ImVec2(8, 8));
    //    }
    //    ImGui::End();

    //    ImGui::Begin("Framerate", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
    //                                  | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
    //                                  | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMouseInputs);
    //    {
    //        ImGui::TextUnformatted(fmt::format(" FPS: {:.0f}", time::fps).c_str());
    //        ImGui::TextUnformatted(fmt::format(" ms: {:.2f}", time::ms).c_str());
    //        ImGui::SetWindowSize(ImVec2(82, 64));
    //        ImGui::SetWindowPos(ImVec2(swapchain.extent.width - ImGui::GetWindowSize().x, 8));
    //    }
    //    ImGui::End();
    //    ImGui::Render();

    //    vk::RenderingAttachmentInfo attach_info {
    //        .imageView = img_view,
    //        .imageLayout = vk::ImageLayout::eGeneral,
    //        .loadOp = vk::AttachmentLoadOp::eLoad,
    //        .storeOp = vk::AttachmentStoreOp::eStore,
    //    };
    //    vk::RenderingInfo render_info {
    //        .renderArea = vk::Rect2D { vk::Offset2D { 0, 0 }, extent },
    //        .layerCount = 1,
    //        .colorAttachmentCount = 1,
    //        .pColorAttachments = &attach_info,
    //        .pDepthAttachment = nullptr,
    //        .pStencilAttachment = nullptr,
    //    };
    //    cmd.beginRendering(&render_info);
    //    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    //    cmd.endRendering();
    //}

} //sigil::renderer::ui

