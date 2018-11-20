#ifndef VKHR_SCENE_GRAPH_HH
#define VKHR_SCENE_GRAPH_HH

#include <vkhr/image.hh>
#include <vkhr/model.hh>
#include <vkhr/light_source.hh>
#include <vkhr/hair_style.hh>
#include <vkhr/camera.hh>

#include <glm/glm.hpp>

#include <list>

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace vkhr {
    class SceneGraph final {
    public:
        SceneGraph() = default;
        SceneGraph(const std::string& file_path);

        // Idea: scene graph traversal is done over here, but the renderers
        // specity how they will render each primitive that is found within
        // the scene graph. They provide a rendering function for each, and
        // each function is called: render_thing(thing, transform, camera).
        template<typename RenderModelFunction, typename RenderHairFunction>
        void traverse(RenderModelFunction render_model,
                      RenderHairFunction  render_hair) const;

        bool load(const std::string& file_path);

        Model& add(Model&& model, const std::string& id);
        Model& add(const Model& model, const std::string& id);
        HairStyle& add(const HairStyle& hair_style, const std::string& id);
        HairStyle& add(HairStyle&& hair_style, const std::string& id);

        HairStyle& add_hair_style(const std::string& file_path);
        Model& add_model(const std::string& file_path);

        void clear();

        using ModelMap     = std::unordered_map<std::string, Model>;
        using HairStyleMap = std::unordered_map<std::string, HairStyle>;

        bool remove(ModelMap::iterator model);
        bool remove(HairStyleMap::iterator hair_style);

        const HairStyleMap& get_hair_styles() const;
        const ModelMap& get_models() const;

        const Camera& get_camera() const;
        Camera& get_camera();

        class Node final {
        public:
            void add(Model* model);
            void add(HairStyle* hair_style);
            void reserve_nodes(std::size_t);
            void add(Node* node);

            bool remove(std::vector<Model*>::iterator model);
            bool remove(std::vector<HairStyle*>::iterator hair_style);
            bool remove(std::vector<Node*>::iterator child);

            const std::vector<Node*>& get_children() const;
            const std::vector<HairStyle*>& get_hair_styles() const;
            const std::vector<Model*>& get_models() const;

            void set_rotation(const glm::vec4& rotation);
            void set_translation(const glm::vec3& translation);
            void set_scale(const glm::vec3& scale);

            const glm::vec4& get_rotation() const;
            const glm::vec3& get_translation() const;
            const glm::vec3& get_scale() const;

            const glm::mat4& get_matrix() const;

            void set_node_name(const std::string& n);
            const std::string& get_node_name() const;

        private:
            void recompute_matrix() const;

            glm::vec4 rotation;
            glm::vec3 translation;
            glm::vec3 scale;

            mutable glm::mat4 matrix;
            bool recalculate_matrix { true };

            std::vector<Node*> children;
            std::vector<HairStyle*> hair_styles;
            std::vector<Model*> models;

            std::string node_name;
        };

        const std::vector<Node>& get_nodes() const;

        Node& push_back_node();
        Node& add(const Node& node);
        Node& add(Node&& node);

        Node& get_root_node();

        enum class Error {
            None,

            OpeningFolder,

            ReadingGraphs,
            ReadingCamera,
            ReadingLights,
            ReadingStyles,
            ReadingModels,
        };

        operator bool() const;
        bool set_error_state(const Error error_state) const;
        Error get_last_error_state() const;

    private:
        Node* root;
        Camera camera;
        std::vector<Node> nodes;
        std::list<LightSource> lights;
        HairStyleMap hair_styles;
        ModelMap models;

        std::string scene_path { "" };

        mutable
        Error error_state {
            Error::None
        };
    };

    template<typename RenderModelFunction, typename RenderHairFunction>
    void SceneGraph::traverse(RenderModelFunction render_model,
                              RenderHairFunction  render_hair) const {
        // TODO: do scene graph traversal over here, hopefully.
    }
}

#endif
