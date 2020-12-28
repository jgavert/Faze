#pragma once

#include <higanbana/core/math/math.hpp>
#include "hittable.hpp"
#include "material.hpp"
#include "rtweekend.hpp"

namespace rt {
class Camera {
public:
Camera(){}
Camera(double3 pos, double3 dir, double3 up, double3 side, double vfov, double aspect_ratio)
  : origin(pos)
  , lower_left_corner(origin)
{
  using namespace higanbana::math;
  auto theta = degrees_to_radians(vfov);
  auto h = tan(theta/2.0);
  auto viewport_height = 2.0 * h;
  auto viewport_width = aspect_ratio * viewport_height;

  auto focal_length = 1.0;

  auto w = normalize(dir);
  auto u = normalize(side);
  auto v = normalize(up);
  horizontal = mul(viewport_width, u);
  vertical = mul(viewport_height, v);
  auto th = div(horizontal, 2.0);
  auto tv = div(vertical, 2.0);
  lower_left_corner = sub(sub(sub(origin, th), tv), w);
}
//double viewport_height;
//double viewport_width;
//double focal_length;

double3 origin;
double3 horizontal;
double3 vertical;
double3 lower_left_corner;

inline Ray get_ray(const double2 uv) {
  double3 t2 = mul(uv.x, horizontal);
  double3 t3 = mul(uv.y, vertical);
  auto dir = sub(add(add(lower_left_corner, t2), t3), origin);
  return rt::Ray(origin, dir);
}

inline double hit_sphere(const double3& center, double radius, const Ray& r) {
  double3 oc = sub(r.origin(), center);
  auto a = length_squared(r.direction());
  auto half_b = dot(oc, r.direction());
  auto c = length_squared(oc) - radius * radius;
  auto discriminant = half_b*half_b - a*c;
  if (discriminant < 0) {
    return -1.0;
  } else {
    return (-half_b - sqrt(discriminant) ) / a;
  }
}

inline double3 ray_color(const Ray& r, const Hittable& world, int depth) {
  HitRecord rec;
  if (depth <= 0)
    return double3(0,0,0);
  if (world.hit(r, 0.001, infinity, rec)) {
    Ray scattered;
    double3 attenuation;
    if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
      return mul(attenuation, ray_color(scattered, world, depth-1));
    return double3(0,0,0);
  }
  double3 unit_direction = normalize(r.direction());
  auto t = 0.5*(unit_direction.y + 1.0);
  return add(mul((1.0-t),double3(1.0, 1.0, 1.0)), mul(t, double3(0.5, 0.7, 1.0)));
}
};
}