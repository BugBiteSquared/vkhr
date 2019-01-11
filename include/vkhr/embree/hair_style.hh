#ifndef VKHR_EMBREE_HAIR_STYLE_HH
#define VKHR_EMBREE_HAIR_STYLE_HH

#include <vkhr/hair_style.hh>

#include <vkhr/embree/shadable.hh>

#include <glm/glm.hpp>

#include <embree3/rtcore.h>

#include <vector>

namespace vkhr {
    class Raytracer;
    namespace embree {
        class HairStyle final : public Shadable {
        public:
            HairStyle() = default;

            HairStyle(const vkhr::HairStyle& hair_style, const vkhr::Raytracer& raytracer);
            void load(const vkhr::HairStyle& hair_style, const vkhr::Raytracer& raytracer);

            glm::vec3 shade(const Ray& surface_intersection,
                            const LightSource& light_source,
                            const Camera& projection_camera) override;

            unsigned get_geometry() const;

        private:
            glm::vec3 kajiya_kay(const glm::vec3& diffuse,
                                 const glm::vec3& specular,
                                 float p,
                                 const glm::vec3& tangent,
                                 const glm::vec3& light,
                                 const glm::vec3& eye);

            unsigned geometry { RTC_INVALID_GEOMETRY_ID };

            std::vector<glm::vec4> position_thickness;
        };
    }
}

#endif