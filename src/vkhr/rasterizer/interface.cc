#include <vkhr/rasterizer/interface.hh>

#include <vkhr/window.hh>
#include <vkhr/scene_graph.hh>
#include <vkhr/rasterizer.hh>

// TODO: wipe when light knobs done!
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <sstream>
#include <iostream>
#include <utility>

namespace vkhr {
    static void imgui_debug_callback(VkResult error) {
        if (error == 0) return; // VK_SUCCESS
        std::cerr << "ImGui error: " << error
                  << std::endl;
    }

    Interface::Interface(Window& window, Rasterizer* vulkan_renderer) {
        IMGUI_CHECKVERSION();
        ctx = ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window.get_handle(), false);
        load(*vulkan_renderer);
    }

    Interface::~Interface() noexcept {
        if (ctx != nullptr) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(ctx);
        }
    }

    void Interface::load(Rasterizer& vulkan_renderer) {
        ImGui_ImplVulkan_InitInfo init_info;

        init_info.Instance = vulkan_renderer.instance.get_handle();
        init_info.PhysicalDevice = vulkan_renderer.physical_device.get_handle();
        init_info.Device = vulkan_renderer.device.get_handle();
        init_info.QueueFamily = vulkan_renderer.physical_device.get_graphics_queue_family_index();
        init_info.Queue = vulkan_renderer.device.get_graphics_queue().get_handle();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = vulkan_renderer.descriptor_pool.get_handle();
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = imgui_debug_callback;

        ImGui_ImplVulkan_Init(&init_info, vulkan_renderer.color_pass.get_handle());

        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        make_custom_style(style);

        auto command_buffer = vulkan_renderer.command_pool.allocate_and_begin();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer.get_handle());
        command_buffer.end();

        vulkan_renderer.device.get_graphics_queue().submit(command_buffer)
                                                   .wait_idle();
        ImGui_ImplVulkan_InvalidateFontUploadObjects();

        default_parameters();
    }

    void Interface::default_parameters() {
        renderers.clear();
        simulations.clear();
        scene_files.clear();
        shaders.clear();

        renderers.push_back("Rasterizer");
        renderers.push_back("Ray Tracer");

        scene_files.push_back(SCENE("ponytail.vkhr"));
        scene_files.push_back(SCENE("bear.vkhr"));

        simulations.push_back("No Effects");

        shaders.push_back("Kajiya-Kay and Blinn-Phong");
        shaders.push_back("Combined Shadow Map and AO");
        shaders.push_back("Local Shadow Map Occlusion");
        shaders.push_back("Ambient Occlusion (Volume)");

        shadow_maps.push_back("Conventional Shadow Maps");
        shadow_maps.push_back("Approximate Deep Shadows");

        shadow_samplers.push_back("  Uniform");
        shadow_samplers.push_back("  Poisson");

        scene_file = simulation_effect = 0;
    }

    void Interface::transform(SceneGraph& scene_graph, Rasterizer& rasterizer, Raytracer& ray_tracer) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (gui_visible) {
            auto& window = rasterizer.window_surface.get_glfw_window();

            ImGui::Begin(" VKHR - a Scalable Strand-Based Hair Renderer",
                         &gui_visible, ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoCollapse);

            if (ImGui::Checkbox("Fullscreen", &window.fullscreen))
                window.toggle_fullscreen(window.fullscreen);

            ImGui::SameLine(0.0f, 10.0f);

            ImGui::Checkbox("VSync", &window.vsync);

            ImGui::SameLine(0.0f, 10.0f);

            if (ImGui::Button("Take Screenshot"))
                rasterizer.get_screenshot(scene_graph, ray_tracer)
                          .save_time();

            ImGui::SameLine();

            ImGui::Button("Help");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Combo("##Renderer",
                         reinterpret_cast<int*>(&current_renderer),
                         get_string_from_vector,
                         static_cast<void*>(&renderers),
                         renderers.size());

            switch (current_renderer) {
            case Renderer::Rasterizer:
                raytrace_scene = false;
                break;
            case Renderer::Ray_Tracer:
                raytrace_scene = true;
                break;
            default: break;
            }

            ImGui::SameLine(0.0, 4.0);

            if (ImGui::Button("Toggle Renderer"))
                toggle_raytracing();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Combo("Reflection Model",
                             reinterpret_cast<int*>(&parameters.shading_model),
                             get_string_from_vector,
                             static_cast<void*>(&shaders),
                             shaders.size())) {
                ray_tracer.visualization_method = static_cast<Raytracer::VisualizationMethod>(parameters.shading_model);
                ray_tracer.now_dirty = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::TreeNodeEx("Rasterizer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushItemWidth(195);
                    ImGui::Combo("##Shadow Technique",
                                 reinterpret_cast<int*>(&parameters.shadow_technique),
                                 get_string_from_vector,
                                 static_cast<void*>(&shadow_maps),
                                 shadow_maps.size());
                    ImGui::PopItemWidth();

                    ImGui::SameLine();

                    if (parameters.shadow_technique == ApproximateDeepShadows) {
                        ImGui::Checkbox("Shadow Maps", reinterpret_cast<bool*>(&parameters.adsm_on));
                    } else if (parameters.shadow_technique == ConventionalShadowMaps) {
                        ImGui::Checkbox("Shadow Maps", reinterpret_cast<bool*>(&parameters.ctsm_on));
                    }

                    ImGui::PushItemWidth(171);
                    if (parameters.shadow_technique == ApproximateDeepShadows) {
                        ImGui::SliderInt("PCF", &parameters.adsm_kernel_size, 1, 9);
                    } else if (parameters.shadow_technique == ConventionalShadowMaps) {
                        ImGui::SliderInt("PCF", &parameters.ctsm_kernel_size, 1, 9);
                    }
                    ImGui::PopItemWidth();

                    ImGui::SameLine();

                    ImGui::PushItemWidth(99);
                    if (parameters.shadow_technique == ApproximateDeepShadows) {
                        ImGui::Combo("##Shadow Sampler",
                                     reinterpret_cast<int*>(&parameters.adsm_sampling_type),
                                     get_string_from_vector,
                                     static_cast<void*>(&shadow_samplers),
                                     shadow_samplers.size());
                    } else if (parameters.shadow_technique == ConventionalShadowMaps) {
                        ImGui::Combo("##Shadow Sampler",
                                     reinterpret_cast<int*>(&parameters.ctsm_sampling_type),
                                     get_string_from_vector,
                                     static_cast<void*>(&shadow_samplers),
                                     shadow_samplers.size());
                    }
                    ImGui::PopItemWidth();

                    ImGui::PushItemWidth(171);
                    if (parameters.shadow_technique == ApproximateDeepShadows) {
                        ImGui::SliderInt("Shadow Map Scaling", &parameters.adsm_stride_size, 1, 15, "%.1f");
                    } else if (parameters.shadow_technique == ConventionalShadowMaps) {
                        if (ImGui::DragFloat("Shadow Bias Values", &parameters.ctsm_bias, 0.0000001f, 0.0f, 0.0f, "%.7f"))
                            parameters.ctsm_bias = std::max(parameters.ctsm_bias, 0.0f);
                    }
                    ImGui::PopItemWidth();

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Ray Tracer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushItemWidth(171);
                    if (ImGui::SliderFloat("AOR", &ray_tracer.ao_radius, 0.00, 5.00))
                        ray_tracer.now_dirty = true;
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Shadow Rays", &ray_tracer.shadows_on))
                        ray_tracer.now_dirty = true;
                    ImGui::TreePop();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Combo("Scene Graph File", &scene_file,
                         get_string_from_vector,
                         static_cast<void*>(&scene_files),
                         scene_files.size());

            // Switch to the new scene by loading it
            if (scene_file != previous_scene_file) {
                scene_graph.load(scene_files[scene_file]);
                rasterizer.load(scene_graph);
                ray_tracer.load(scene_graph);
                previous_scene_file = scene_file;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Scene Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::TreeNode("Camera")) {
                    if (ImGui::SliderAngle("Field of View", &scene_graph.camera.field_of_view, 0.0f, 180.0f))
                        scene_graph.camera.recalculate_projection_matrix();
                    if (ImGui::DragFloat3("View Position", glm::value_ptr(scene_graph.camera.position)))
                        scene_graph.camera.set_position(scene_graph.camera.position);
                    if (ImGui::DragFloat3("Look at Point", glm::value_ptr(scene_graph.camera.look_at_point)))
                        scene_graph.camera.set_look_at_point(scene_graph.camera.look_at_point);
                    if (ImGui::DragFloat("View Distance", &scene_graph.camera.distance, 1.0f, 0.0f, 0.0f, "%.2f"))
                        scene_graph.camera.set_distance(scene_graph.camera.distance);

                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Lights")) {
                    for (auto& light : scene_graph.light_sources) {
                        if (ImGui::TreeNode(light.get_type_name().c_str())) {
                            if (ImGui::ColorEdit3("Highlights", glm::value_ptr(light.buffer.intensity), ImGuiColorEditFlags_Float))
                                ray_tracer.now_dirty = true;
                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }

                traverse(scene_graph);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Recompile Shaders"))
                rasterizer.recompile();

            ImGui::SameLine();

            ImGui::Checkbox("Display Sources of Light", &light_debugger);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Combo("Animation Effect",
                         &simulation_effect,
                         get_string_from_vector,
                         static_cast<void*>(&simulations),
                         simulations.size());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Shader Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& profile : profiles)
                    ImGui::PlotLines(profile.first.c_str(),
                                     profile.second.timestamps.data(),
                                     profile.second.timestamps.size(),
                                     profile.second.offset,
                                     profile.second.output.c_str());
            }

            ImGui::End();
        }

        if (scene_graph.camera.viewing_plane_dirty)
            ray_tracer.now_dirty = true;

        ImGui::Render();
    }

    void Interface::traverse(SceneGraph& scene_graph) {
        traverse(&scene_graph.get_root_node());
    }

    void Interface::traverse(SceneGraph::Node* node) {
        if (node == nullptr) {
            return;
        }

        if (ImGui::TreeNode(node->get_node_name().c_str())) {
            if (ImGui::TreeNode("Transforms")) {
                if (ImGui::DragFloat3("T", glm::value_ptr(node->translation), 0.1f))
                    node->set_translation(node->translation);
                if (ImGui::DragFloat3("R", glm::value_ptr(node->rotation_axis), 0.01f))
                    node->set_rotation_axis(node->rotation_axis);
                if (ImGui::DragFloat("##angle", &node->rotation_angle, 0.01f))
                    node->set_rotation_angle(node->rotation_angle);
                if (ImGui::DragFloat3("S", glm::value_ptr(node->scaling), 0.01))
                    node->set_scale(node->scaling);
                ImGui::TreePop();
            }

            for (auto hair_style : node->get_hair_styles()) {
                if (ImGui::TreeNode("Hair Style")) {
                    if (ImGui::TreeNode("Headers")) {
                        ImGui::Indent();
                        ImGui::Text("magic id: H A I R");
                        ImGui::Text("%.4f megabytes", hair_style->get_size() / static_cast<float>(1 << 20));
                        ImGui::Text("vertices: %d", hair_style->get_vertex_count());
                        ImGui::Text("segments: %d", hair_style->get_segment_count());
                        ImGui::Text("%d segment/strand", hair_style->get_default_segment_count());
                        ImGui::Text("strands!: %d", hair_style->get_strand_count());
                        ImGui::Text("feature bitfield:");
                        ImGui::Text("0 1 0 0 0 1 1 1 0");
                        ImGui::TreePop();
                        ImGui::Unindent();
                    }

                    ImGui::TreePop();
                }
            }

            for (auto child : node->get_children())
                traverse(child);

            ImGui::TreePop();
        }
    }

    void Interface::draw(vkpp::CommandBuffer& command_buffer, vkpp::QueryPool& query_pool) {
        if (gui_visible) {
            vk::DebugMarker::begin(command_buffer, "Draw GUI Overlay", query_pool);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                            command_buffer.get_handle());
            vk::DebugMarker::close(command_buffer, "Draw GUI Overlay", query_pool);
        }
    }

    void Interface::draw(vkpp::CommandBuffer& command_buffer) {
        if (gui_visible) {
            vk::DebugMarker::begin(command_buffer, "Draw GUI Overlay");
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                            command_buffer.get_handle());
            vk::DebugMarker::close(command_buffer);
        }
    }

    bool Interface::wants_focus() const {
        return ImGui::GetIO().WantCaptureMouse    && gui_visible;
    }

    bool Interface::typing_text() const {
        return ImGui::GetIO().WantCaptureKeyboard && gui_visible;
    }

    void Interface::set_visibility(bool visible) {
        gui_visible = visible;
    }

    bool Interface::hide() {
        auto previous_visibility = gui_visible;
        gui_visible = false;
        return previous_visibility;
    }

    void Interface::toggle_visibility() {
        gui_visible = !gui_visible;
    }

    void Interface::toggle_raytracing() {
        raytrace_scene = !raytrace_scene;
        if (raytrace_scene)
            current_renderer = Renderer::Ray_Tracer;
        else
            current_renderer = Renderer::Rasterizer;
    }

    bool Interface::show() {
        auto previous_visibility = gui_visible;
        gui_visible = true;
        return previous_visibility;
    }

    bool Interface::raytracing_enabled() {
        return raytrace_scene;
    }

    Interface::Interface(Interface&& interface) noexcept {
        swap(*this, interface);
    }

    Interface& Interface::operator=(Interface&& interface) noexcept {
        swap(*this, interface);
        return *this;
    }

    void swap(Interface& lhs, Interface& rhs) {
        using std::swap;

        swap(lhs.ctx, rhs.ctx);
        swap(lhs.gui_visible, rhs.gui_visible);
        swap(lhs.raytrace_scene, rhs.raytrace_scene);

        swap(lhs.scene_file, rhs.scene_file);
        swap(lhs.scene_files, rhs.scene_files);
        swap(lhs.renderers, rhs.renderers);
        swap(lhs.simulations, rhs.simulations);
        swap(lhs.simulation_effect, rhs.simulation_effect);
        swap(lhs.shaders, rhs.shaders);
        swap(lhs.shadow_maps, rhs.shadow_maps);
        swap(lhs.shadow_samplers, rhs.shadow_samplers);

        swap(lhs.profiles, rhs.profiles);

        swap(lhs.light_debugger, rhs.light_debugger);
    }

    void Interface::make_custom_style(ImGuiStyle& style) {
        style.FrameRounding  = 2.0f;
        style.GrabRounding   = 2.0f;
    }

    bool Interface::get_string_from_vector(void* data, int n, const char** str) {
        std::vector<std::string>* v = (std::vector<std::string>*) data;
        *str = v->at(n).c_str();
        return true;
    }

    void Interface::record_performance(const std::unordered_map<std::string, float>& timestamps) {
        for (auto profile = profiles.begin(); profile != profiles.end();) {
            if (timestamps.find(profile->first) == timestamps.end())
                profiles.erase(profile++);
            else profile++;
        }

        for (const auto& timestamp : timestamps) {
            auto profile = profiles.find(timestamp.first);
            if (profile == profiles.end()) {
                profiles[timestamp.first] = ProfilePair { {}, 0 };
                profile = profiles.find(timestamp.first);
                profile->second.timestamps.resize(profile_limit);
                std::fill(profile->second.timestamps.begin(),
                          profile->second.timestamps.end(), 0.0f);
            }

            profile->second.timestamps[profile->second.offset] = timestamp.second;

            if (profile->second.offset == (profile_limit - 1)) {
                auto sum = std::accumulate(profile->second.timestamps.begin(),
                                           profile->second.timestamps.end(), 0.0f);
                auto average = sum / profile->second.timestamps.size();
                profile->second.output.resize(10);
                std::snprintf(&profile->second.output[0], 10, "%5.2fms", average);
            }

            profile->second.offset = (profile->second.offset + 1) % profile_limit;
        }
    }
}