#pragma once
#include "vulkan.hh"
#include "glfw.hh"

namespace sigil::renderer {

    inline constexpr glm::vec3 world_up = glm::vec3(0.f, 0.f, -1.f);

    //_____________________________________
    struct Camera {
        struct {
            glm::vec3 position = glm::vec3( -1.8f, 1.8f, -0.6f );
            glm::vec3 rotation = glm::vec3( -45.f, 13.f, 0.f );
            glm::vec3 scale    = glm::vec3( 0 );
        } transform;
        float yaw   = transform.rotation.x;
        float pitch = transform.rotation.y;
        float fov   { 70.f  };
        glm::vec3 velocity;
        glm::vec3 forward_vector = glm::normalize(glm::vec3(
            /* x */ cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            /* y */ sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
            /* z */ sin(glm::radians(pitch))
        ));
        glm::vec3 up_vector      = -world_up;
        glm::vec3 right_vector   = glm::cross(forward_vector, world_up);
        struct {
            float near =  0.01f;
            float far  = 256.0f;
        } clip_plane;
        struct {
            bool forward;
            bool right;
            bool back; 
            bool left; 
            bool up; 
            bool down; 
        } request_movement;
        float movement_speed = 1.f;
        bool follow_mouse = false;
        float mouse_sens = 24.f;

        inline void update(float delta_time) {
            if( request_movement.forward ) velocity += forward_vector;
            if( request_movement.back    ) velocity -= forward_vector;
            if( request_movement.right   ) velocity -= right_vector;
            if( request_movement.left    ) velocity += right_vector;
            if( request_movement.up      ) velocity -= up_vector;
            if( request_movement.down    ) velocity += up_vector;

            transform.position += velocity * movement_speed * delta_time;
            velocity = glm::vec3(0);

            if( follow_mouse ) {
                glm::dvec2 offset = -input::get_mouse_movement() * glm::dvec2(mouse_sens) * glm::dvec2(delta_time);
                yaw += offset.x;
                pitch = ( pitch - offset.y >  89.f ?  89.f
                        : pitch - offset.y < -89.f ? -89.f
                        : pitch - offset.y);
                forward_vector = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
                right_vector = glm::cross(forward_vector, world_up);
                up_vector = glm::cross(forward_vector, right_vector);
                //transform.rotation = glm::qrot(transform.rotation, forward_vector);
            }
        }

        inline glm::mat4 get_view() {
            return glm::lookAt(transform.position, (transform.position + forward_vector), up_vector);
        }

    } inline camera;

} // sigil::renderer

