// This file is distributed under the MIT license.
// See the LICENSE file for details.

#include <visionaray/math/forward.h>
#include <visionaray/math/vector.h>
#include <visionaray/pinhole_camera.h>

#include "../input/mouse_event.h"

#include "zoom_manipulator.h"


using namespace visionaray;


zoom_manipulator::zoom_manipulator(
        pinhole_camera& cam,
        mouse::buttons buttons,
        keyboard::key_modifiers modifiers
        )
    : camera_manipulator(cam)
    , buttons_(buttons)
    , modifiers_(modifiers)
    , dragging_(false)
    , down_modifiers_(keyboard::key_modifiers::NoKey)
{
}


zoom_manipulator::~zoom_manipulator()
{
}


void zoom_manipulator::handle_mouse_down(visionaray::mouse_event const& event)
{

    bool buttons   = event.buttons() & buttons_;
    bool modifiers = (modifiers_ == keyboard::NoKey && event.modifiers() == keyboard::NoKey)
                   || event.modifiers()  & modifiers_;

    if (!dragging_ && buttons && modifiers)
    {
        dragging_ = true;

        // TODO: do this in base?
        last_pos_ = event.pos();
        down_modifiers_ = event.modifiers();
    }

    camera_manipulator::handle_mouse_down(event);

}


void zoom_manipulator::handle_mouse_up(visionaray::mouse_event const& event)
{

    dragging_ = false;
    down_modifiers_ = keyboard::NoKey;

    camera_manipulator::handle_mouse_up(event);

}


void zoom_manipulator::handle_mouse_move(visionaray::mouse_event const& event)
{

    bool buttons   = event.buttons() & buttons_;
    bool modifiers = (modifiers_ == keyboard::NoKey && down_modifiers_ == keyboard::NoKey) || down_modifiers_ & modifiers_;

    if (dragging_ && buttons && modifiers)
    {

//      float w  = camera_.get_viewport().w;
        float h  = camera_.get_viewport().h;
//      float dx =  static_cast<float>(last_pos_.x - event.pos().x) / w;
        float dy = -static_cast<float>(last_pos_.y - event.pos().y) / h;
        float s  = 2.0f * camera_.distance() * dy;

        vec3 dir = normalize( camera_.eye() - camera_.center() );

        camera_.look_at( camera_.eye() - dir * s, camera_.center(), camera_.up() );

        last_pos_ = event.pos();

    }

    camera_manipulator::handle_mouse_move(event);

}
