// This file is distributed under the MIT license.
// See the LICENSE file for details.

#pragma once

#ifndef VSNRAY_DETAIL_PATHTRACING_INL
#define VSNRAY_DETAIL_PATHTRACING_INL 1

#include <visionaray/get_surface.h>
#include <visionaray/result_record.h>
#include <visionaray/sampling.h>
#include <visionaray/spectrum.h>
#include <visionaray/surface_interaction.h>
#include <visionaray/traverse.h>

namespace visionaray
{
namespace pathtracing
{

template <typename Params>
struct kernel
{

    Params params;

    template <typename Intersector, typename R, typename Generator>
    VSNRAY_FUNC result_record<typename R::scalar_type> operator()(
            Intersector& isect,
            R ray,
            Generator& gen
            ) const
    {
        using S = typename R::scalar_type;
        using I = simd::int_type_t<S>;
        using V = typename result_record<S>::vec_type;
        using C = spectrum<S>;

        simd::mask_type_t<S> active_rays = true;
        simd::mask_type_t<S> search_light = true;

        C intensity(0.0);
        C throughput(1.0);

        result_record<S> result;
        result.color = params.bg_color;

        for (unsigned bounce = 0; bounce < params.num_bounces; ++bounce)
        {
            auto hit_rec = closest_hit(ray, params.prims.begin, params.prims.end, isect);

            // Handle rays that just exited
            auto exited = active_rays & !hit_rec.hit;
            intensity += select(
                exited,
                C(from_rgba(params.ambient_color)) * throughput,
                C(0.0)
                );


            // Exit if no ray is active anymore
            active_rays &= hit_rec.hit;

            if (!any(active_rays))
            {
                break;
            }

            // Special handling for first bounce
            if (bounce == 0)
            {
                result.hit = hit_rec.hit;
                result.isect_pos = ray.ori + ray.dir * hit_rec.t;
            }


            // Process the current bounce

            V refl_dir;
            V view_dir = -ray.dir;

            hit_rec.isect_pos = ray.ori + ray.dir * hit_rec.t;

            auto surf = get_surface(hit_rec, params);

            S pdf(0.0);

            // Remember the last type of surface interaction.
            // If the last interaction was not diffuse, we have
            // to include light from emissive surfaces.
            I inter = 0;
            auto src = surf.sample(view_dir, refl_dir, pdf, inter, gen);

            auto zero_pdf = pdf <= S(0.0);

            intensity += select(
                active_rays && search_light && inter == surface_interaction::Emission,
                throughput * src,
                C(0.0)
                );

            active_rays &= inter != surface_interaction::Emission;
            active_rays &= !zero_pdf;

            if (params.lights.end - params.lights.begin > 0 && any(inter == surface_interaction::Diffuse))
            {
                auto ls = sample_random_light(params.lights.begin, params.lights.end, gen);

                float num_lights = params.lights.end - params.lights.begin;

                search_light &= (ls.delta_light || inter != surface_interaction::Diffuse);

                auto ld = length(ls.pos - hit_rec.isect_pos);
                auto L = normalize(ls.pos - hit_rec.isect_pos);

                auto n = surf.shading_normal;
#if 1
                n = faceforward( n, view_dir, surf.geometric_normal );
#endif

                auto ln = ls.normal;
#if 1
                ln = faceforward( ln, -L, ln );
#endif
                auto ldotn = dot(L, n);
                auto ldotln = abs(dot(-L, ln));

                R shadow_ray(
                    hit_rec.isect_pos + L * S(params.epsilon),
                    L
                    );

                auto lhr = any_hit(shadow_ray, params.prims.begin, params.prims.end, ld - S(2.0f * params.epsilon), isect);

                auto brdf_pdf = surf.pdf(view_dir, L, inter);

                auto solid_angle = (ldotln * ls.area) / (ld * ld);
                auto light_pdf = S(1.0) / solid_angle;

                intensity += select(
                    active_rays && !lhr.hit && inter == surface_interaction::Diffuse && ldotn > S(0.0) && ldotln > S(0.0),
                    throughput * (src / (dot(n, refl_dir) / pdf)) * (ldotn / light_pdf) * from_rgb(ls.intensity) * S(num_lights),
                    C(0.0)
                    );
            }

            throughput = mul( throughput, src, !zero_pdf, throughput );
            throughput = select( zero_pdf, C(0.0), throughput );

            if (!any(active_rays))
            {
                break;
            }

            ray.ori = hit_rec.isect_pos + refl_dir * S(params.epsilon);
            ray.dir = refl_dir;
        }

        result.color = select( result.hit, to_rgba(intensity), result.color );

        return result;
    }

    template <typename R, typename Generator>
    VSNRAY_FUNC result_record<typename R::scalar_type> operator()(
            R ray,
            Generator& gen
            ) const
    {
        default_intersector ignore;
        return (*this)(ignore, ray, gen);
    }
};

} // pathtracing
} // visionaray

#endif // VSNRAY_DETAIL_PATHTRACING_INL
