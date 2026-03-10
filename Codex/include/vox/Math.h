#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vox {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3i {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3f& operator+=(const Vec3f& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vec3f& operator-=(const Vec3f& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    Vec3f& operator*=(float s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vec3f& operator/=(float s) {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
};

inline Vec3f operator+(Vec3f lhs, const Vec3f& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec3f operator-(Vec3f lhs, const Vec3f& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec3f operator*(Vec3f lhs, float s) {
    lhs *= s;
    return lhs;
}

inline Vec3f operator*(float s, Vec3f rhs) {
    rhs *= s;
    return rhs;
}

inline Vec3f operator/(Vec3f lhs, float s) {
    lhs /= s;
    return lhs;
}

inline Vec3f operator*(Vec3f lhs, const Vec3f& rhs) {
    lhs.x *= rhs.x;
    lhs.y *= rhs.y;
    lhs.z *= rhs.z;
    return lhs;
}

inline float dot(const Vec3f& a, const Vec3f& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3f cross(const Vec3f& a, const Vec3f& b) {
    return Vec3f{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float length(const Vec3f& v) {
    return std::sqrt(dot(v, v));
}

inline Vec3f normalize(const Vec3f& v) {
    const float len = length(v);
    return len > 0.0f ? v / len : Vec3f{};
}

inline float clampf(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

inline float saturate(float value) {
    return clampf(value, 0.0f, 1.0f);
}

inline Vec3f lerp(const Vec3f& a, const Vec3f& b, float t) {
    return a + (b - a) * t;
}

inline float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

inline std::uint8_t to_u8(float normalized) {
    return static_cast<std::uint8_t>(clampf(normalized * 255.0f + 0.5f, 0.0f, 255.0f));
}

inline float srgb_from_linear(float linear) {
    return std::pow(saturate(linear), 1.0f / 2.2f);
}

inline float linear_from_srgb(float srgb) {
    return std::pow(saturate(srgb), 2.2f);
}

} // namespace vox
