// This file is distributed under the MIT license.
// See the LICENSE file for details.

#pragma once

#ifndef VSNRAY_DETAIL_SCHED_COMMON_H
#define VSNRAY_DETAIL_SCHED_COMMON_H 1

#include <chrono>

#include <visionaray/mc.h>
#include <visionaray/pixel_format.h>
#include <visionaray/render_target.h>
#include <visionaray/result_record.h>
#include <visionaray/scheduler.h>

#include "macros.h"
#include "pixel_access.h"
#include "tags.h"

namespace visionaray
{
namespace detail
{

//-------------------------------------------------------------------------------------------------
// TODO: move to a better place
//

VSNRAY_FUNC
inline unsigned tic()
{
#if defined(__CUDA_ARCH__)
    return clock64();
#else
    auto t = std::chrono::high_resolution_clock::now();
    return t.time_since_epoch().count();
#endif
}


//-------------------------------------------------------------------------------------------------
// make primary ray
//

template <typename R, typename T, typename V>
VSNRAY_FUNC
inline R make_primary_ray_impl(
        T                   x,
        T                   y,
        V const&            viewport,
        vector<3, T> const& eye,
        vector<3, T> const& cam_u,
        vector<3, T> const& cam_v,
        vector<3, T> const& cam_w
        )
{
    auto u = T(2.0) * (x + T(0.5)) / T(viewport.w) - T(1.0);
    auto v = T(2.0) * (y + T(0.5)) / T(viewport.h) - T(1.0);

    R r;
    r.ori = eye;
    r.dir = normalize(cam_u * u + cam_v * v + cam_w);
    return r;
}

template <typename R, typename T, typename V>
VSNRAY_FUNC
inline R make_primary_ray_impl(
        T                       x,
        T                       y,
        V const&                viewport,
        matrix<4, 4, T> const&  view_matrix,
        matrix<4, 4, T> const&  inv_view_matrix,
        matrix<4, 4, T> const&  proj_matrix,
        matrix<4, 4, T> const&  inv_proj_matrix
        )
{
    VSNRAY_UNUSED(view_matrix);
    VSNRAY_UNUSED(proj_matrix);

    auto u = T(2.0) * (x + T(0.5)) / T(viewport.w) - T(1.0);
    auto v = T(2.0) * (y + T(0.5)) / T(viewport.h) - T(1.0);

    auto o = inv_view_matrix * ( inv_proj_matrix * vector<4, T>(u, v, -1,  1) );
    auto d = inv_view_matrix * ( inv_proj_matrix * vector<4, T>(u, v,  1,  1) );

    R r;
    r.ori =            o.xyz() / o.w;
    r.dir = normalize( d.xyz() / d.w - r.ori );
    return r;
}

template <typename R, typename Sampler, typename ...Args>
VSNRAY_FUNC
inline R make_primary_ray(
        pixel_sampler::uniform_type /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    VSNRAY_UNUSED(samp);

    using S = typename R::scalar_type;
    return make_primary_ray_impl<R>(pixel<S>().x(x), pixel<S>().y(y), args...);
}

template <typename R, typename Sampler, typename ...Args>
VSNRAY_FUNC
inline R make_primary_ray(
        pixel_sampler::jittered_type /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    using S = typename R::scalar_type;

    vector<2, S> jitter( samp.next() - S(0.5), samp.next() - S(0.5) );

    return make_primary_ray_impl<R>(
            pixel<S>().x(x) + jitter.x,
            pixel<S>().y(y) + jitter.y,
            args...
            );
}


// 2x SSAA ------------------------------------------------

template <typename R, typename Sampler, typename ...Args>
VSNRAY_FUNC
inline std::array<R, 2> make_primary_ray(
        pixel_sampler::ssaa_type<2> /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    VSNRAY_UNUSED(samp);

    using S = typename R::scalar_type;

    return {{
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.25), pixel<S>().y(y) - S(0.25), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.25), pixel<S>().y(y) + S(0.25), args...),
        }};
}

// 4x SSAA ------------------------------------------------

template <typename R, typename Sampler, typename ...Args>
VSNRAY_FUNC
inline std::array<R, 4> make_primary_ray(
        pixel_sampler::ssaa_type<4> /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    VSNRAY_UNUSED(samp);

    using S = typename R::scalar_type;

    return {{
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.125), pixel<S>().y(y) - S(0.375), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.375), pixel<S>().y(y) - S(0.125), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.125), pixel<S>().y(y) + S(0.375), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.375), pixel<S>().y(y) + S(0.125), args...)
        }};
}

// 8x SSAA ------------------------------------------------

template <typename R, typename Sampler, typename ...Args>
VSNRAY_FUNC
inline std::array<R, 8> make_primary_ray(
        pixel_sampler::ssaa_type<8> /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    VSNRAY_UNUSED(samp);

    using S = typename R::scalar_type;

    return {{
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.125), pixel<S>().y(y) - S(0.4375), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.375), pixel<S>().y(y) - S(0.3125), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.375), pixel<S>().y(y) - S(0.1875), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.125), pixel<S>().y(y) - S(0.0625), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.125), pixel<S>().y(y) + S(0.0625), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.375), pixel<S>().y(y) + S(0.1825), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) - S(0.375), pixel<S>().y(y) + S(0.3125), args...),
        make_primary_ray_impl<R>(pixel<S>().x(x) + S(0.125), pixel<S>().y(y) + S(0.4375), args...)
        }};
}


template <
    typename R,
    size_t   Num,
    typename PxSamplerT,
    typename Sampler,
    typename ...Args
    >
VSNRAY_FUNC
inline std::array<R, Num> make_primary_rays(
        PxSamplerT /* */,
        Sampler& samp,
        unsigned x,
        unsigned y,
        Args&&... args
        )
{
    std::array<R, Num> result;

    for (size_t i = 0; i < Num; ++i)
    {
        result[i] = make_primary_ray<R>(
                PxSamplerT(),
                samp,
                x,
                y,
                std::forward<Args>(args)...
                );
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// Depth transform
//

template <typename T>
VSNRAY_FUNC inline T depth_transform(
        vector<3, T> const&     isect_pos,
        matrix<4, 4, T> const&  view_matrix,
        matrix<4, 4, T> const&  inv_view_matrix,
        matrix<4, 4, T> const&  proj_matrix,
        matrix<4, 4, T> const&  inv_proj_matrix
        )
{
    VSNRAY_UNUSED(inv_view_matrix);
    VSNRAY_UNUSED(inv_proj_matrix);

    auto pos4 = proj_matrix * (view_matrix * vector<4, T>(isect_pos, T(1.0)));
    auto pos3 = pos4.xyz() / pos4.w;

    return (pos3.z + T(1.0)) * T(0.5);
}


//-------------------------------------------------------------------------------------------------
// Simple uniform pixel sampler
//

template <
    typename K,
    typename R,
    typename Sampler,
    pixel_format CF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                               kernel,
        pixel_sampler::uniform_type     /* */,
        R const&                        r,
        Sampler&                        samp,
        unsigned                        frame_num,
        render_target_ref<CF>           rt_ref,
        unsigned                        x,
        unsigned                        y,
        V const&                        viewport,
        Args&&...                       args
        )
{
    VSNRAY_UNUSED(samp);
    VSNRAY_UNUSED(frame_num);
    VSNRAY_UNUSED(args...);

    auto result = kernel(r);
    pixel_access::store(x, y, viewport, result, rt_ref.color());
}

template <
    typename K,
    typename R,
    typename Sampler,
    pixel_format CF,
    pixel_format DF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                               kernel,
        pixel_sampler::uniform_type     /* */,
        R const&                        r,
        Sampler&                        samp,
        unsigned                        frame_num,
        render_target_ref<CF, DF>       rt_ref,
        unsigned                        x,
        unsigned                        y,
        V const&                        viewport,
        Args&&...                       args
        )
{
    VSNRAY_UNUSED(samp);
    VSNRAY_UNUSED(frame_num);

    auto result = kernel(r);
    result.depth = select( result.hit, depth_transform(result.isect_pos, args...), typename R::scalar_type(1.0) );
    pixel_access::store(x, y, viewport, result, rt_ref.color(), rt_ref.depth());
}


//-------------------------------------------------------------------------------------------------
// Jittered pixel sampler, sampler is passed to kernel
//

template <
    typename K,
    typename R,
    typename Sampler,
    pixel_format CF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                               kernel,
        pixel_sampler::jittered_type    /* */,
        R const&                        r,
        Sampler&                        samp,
        unsigned                        frame_num,
        render_target_ref<CF>           rt_ref,
        unsigned                        x,
        unsigned                        y,
        V const&                        viewport,
        Args&&...                       args
        )
{
    VSNRAY_UNUSED(frame_num);
    VSNRAY_UNUSED(args...);

    auto result = kernel(r, samp);
    pixel_access::store(x, y, viewport, result, rt_ref.color());
}


//-------------------------------------------------------------------------------------------------
// Jittered pixel sampler, result is blended on top of color buffer, sampler is passed to kernel
//

template <
    typename K,
    typename R,
    typename Sampler,
    pixel_format CF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                                   kernel,
        pixel_sampler::jittered_blend_type  /* */,
        R const&                            r,
        Sampler&                            samp,
        unsigned                            frame_num,
        render_target_ref<CF>               rt_ref,
        unsigned                            x,
        unsigned                            y,
        V const&                            viewport,
        Args&&...                           args
        )
{
    VSNRAY_UNUSED(args...);

    using S     = typename R::scalar_type;
    using Color = typename decltype(kernel(r, samp))::color_type;

    auto result = kernel(r, samp);
    auto alpha  = S(1.0) / S(frame_num);
    if (frame_num <= 1)
    {//TODO: clear method in render target?
        pixel_access::store(x, y, viewport, Color(0.0), rt_ref.color());
    }
    pixel_access::blend(x, y, viewport, result, rt_ref.color(), alpha, S(1.0) - alpha);
}

template <
    typename K,
    typename R,
    typename Sampler,
    pixel_format CF,
    pixel_format DF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                                   kernel,
        pixel_sampler::jittered_blend_type  /* */,
        R const&                            r,
        Sampler&                            samp,
        unsigned                            frame_num,
        render_target_ref<CF, DF>           rt_ref,
        unsigned                            x,
        unsigned                            y,
        V const&                            viewport,
        Args&&...                           args
        )
{
    using S = typename R::scalar_type;

    auto result = kernel(r, samp);
    auto alpha  = S(1.0) / S(frame_num);

    result.depth = select( result.hit, depth_transform(result.isect_pos, args...), S(1.0) );

    if (frame_num <= 1)
    {//TODO: clear method in render target?
        pixel_access::store(x, y, viewport, result, rt_ref.color(), rt_ref.depth());
    }
    pixel_access::blend(x, y, viewport, result, rt_ref.color(), rt_ref.depth(), alpha, S(1.0) - alpha);
}


//-------------------------------------------------------------------------------------------------
// jittered pixel sampler, blends several samples at once
//

template <
    typename K,
    typename R,
    size_t Num,
    typename Sampler,
    pixel_format CF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                                   kernel,
        pixel_sampler::jittered_blend_type  /* */,
        std::array<R, Num> const&           rays,
        Sampler&                            samp,
        unsigned                            frame_num,
        render_target_ref<CF>               rt_ref,
        unsigned                            x,
        unsigned                            y,
        V const&                            viewport,
        Args&&...                           args
        )
{
    VSNRAY_UNUSED(args...);

    using S     = typename R::scalar_type;
    using Color = typename decltype(kernel(R(), samp))::color_type;

    auto ray_ptr = rays.data();

    auto frame_begin = frame_num;
    auto frame_end   = frame_num + Num;

    for (size_t frame = frame_begin; frame < frame_end; ++frame)
    {
        if (frame <= 1)
        {//TODO: clear method in render target?
            pixel_access::store(x, y, viewport, Color(0.0), rt_ref.color());
        }

        auto result = kernel(*ray_ptr++, samp);
        auto alpha = S(1.0) / S(frame);
        pixel_access::blend(x, y, viewport, result, rt_ref.color(), alpha, S(1.0) - alpha);
    }
}


//-------------------------------------------------------------------------------------------------
// SSAA pixel sampler
//

template <
    typename K,
    typename R,
    size_t Num,
    typename Sampler,
    pixel_format CF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                               kernel,
        pixel_sampler::ssaa_type<Num>   /* */,
        std::array<R, Num> const&       rays,
        Sampler&                        samp,
        unsigned                        frame_num,
        render_target_ref<CF>           rt_ref,
        unsigned                        x,
        unsigned                        y,
        V const&                        viewport,
        Args&&...                       args
        )
{
    VSNRAY_UNUSED(samp);
    VSNRAY_UNUSED(frame_num);
    VSNRAY_UNUSED(args...);

    using S     = typename R::scalar_type;
    using Color = typename decltype(kernel(R()))::color_type;

    auto ray_ptr = rays.data();

    auto frame_begin = 0;
    auto frame_end   = Num;

    //TODO: clear method in render target?
    pixel_access::store(x, y, viewport, Color(0.0), rt_ref.color());

    for (size_t frame = frame_begin; frame < frame_end; ++frame)
    {
        auto result = kernel(*ray_ptr++);
        auto alpha = S(1.0) / S(Num);
        pixel_access::blend(x, y, viewport, result, rt_ref.color(), alpha, S(1.0));
    }
}

template <
    typename K,
    typename R,
    size_t Num,
    typename Sampler,
    pixel_format CF,
    pixel_format DF,
    typename V,
    typename ...Args
    >
VSNRAY_FUNC
inline void sample_pixel_impl(
        K                               kernel,
        pixel_sampler::ssaa_type<Num>   /* */,
        std::array<R, Num> const&       rays,
        Sampler&                        samp,
        unsigned                        frame_num,
        render_target_ref<CF, DF>       rt_ref,
        unsigned                        x,
        unsigned                        y,
        V const&                        viewport,
        Args&&...                       args
        )
{
    VSNRAY_UNUSED(samp);
    VSNRAY_UNUSED(args...);

    using S      = typename R::scalar_type;
    using Result = decltype(kernel(R{}));

    auto ray_ptr = rays.data();

    auto frame_begin = frame_num;
    auto frame_end   = frame_num + Num;

    //TODO: clear method in render target?
    pixel_access::store(x, y, viewport, Result{}, rt_ref.color(), rt_ref.depth());

    for (size_t frame = frame_begin; frame < frame_end; ++frame)
    {
        auto result = kernel(*ray_ptr++);
        result.depth = select( result.hit, depth_transform(result.isect_pos, args...), S(1.0) );
        auto alpha = S(1.0) / S(Num);
        pixel_access::blend(x, y, viewport, result, rt_ref.color(), rt_ref.depth(), alpha, S(1.0));
    }
}

//-------------------------------------------------------------------------------------------------
// w/o intersector
//

template <typename ...Args>
VSNRAY_FUNC
inline void sample_pixel_choose_intersector_impl(Args&&... args)
{
    detail::sample_pixel_impl(std::forward<Args>(args)...);
}


//-------------------------------------------------------------------------------------------------
// w/ intersector
//

template <typename K, typename I>
struct call_kernel_with_intersector
{
    K& kernel;
    I& isect;

    VSNRAY_FUNC
    explicit call_kernel_with_intersector(K& k, I& i) : kernel(k) , isect(i) {}

    template <typename ...Args>
    VSNRAY_FUNC
    auto operator()(Args&&... args) const
        -> decltype( kernel(isect, std::forward<Args>(args)...) )
    {
        return kernel(isect, std::forward<Args>(args)...);
    }
};

template <typename K, typename Intersector, typename ...Args>
VSNRAY_FUNC
inline void sample_pixel_choose_intersector_impl(
        have_intersector_tag    /* */,
        Intersector&            isect,
        K                       kernel,
        Args&&...               args
        )
{
    auto caller = call_kernel_with_intersector<K, Intersector>(kernel, isect);
    detail::sample_pixel_impl(
            caller,
            std::forward<Args>(args)...
            );
}

} // detail


//-------------------------------------------------------------------------------------------------
// Dispatch pixel sampler
//

template <typename ...Args>
VSNRAY_FUNC
inline void sample_pixel(Args&&...  args)
{
    detail::sample_pixel_choose_intersector_impl(std::forward<Args>(args)...);
}

} // visionaray

#endif // VSNRAY_DETAIL_SCHED_COMMON_H
