#include "vec.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>


namespace {
    constexpr float PI = 3.14159265358979323846f;
    
    float normalize_angle(float angle) {
        angle = std::fmod(angle, 2.0f * PI);
        if (angle > PI) angle -= 2.0f * PI;
        else if (angle < -PI) angle += 2.0f * PI;
        return angle;
    }
}

Vec2i Vec2i::operator/(const Vec2i& o) const {
    if (o.x == 0 || o.y == 0) throw std::domain_error("Division by zero");
    return Vec2i(x / o.x, y / o.y);
}

Vec2i Vec2i::operator/(int32_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec2i(x / s, y / s);
}

float Vec2i::length() const { return std::sqrt(static_cast<float>(x * x + y * y)); }

Vec2i Vec2i::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec2i(0, 0);
    return Vec2i(static_cast<int32_t>(std::round(x / len)), static_cast<int32_t>(std::round(y / len)));
}

void Vec2i::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = static_cast<int32_t>(std::round(x * scale));
    y = static_cast<int32_t>(std::round(y * scale));
}

Vec2f Vec2i::to_f() const { return Vec2f(static_cast<float>(x), static_cast<float>(y)); }
Vec2u8 Vec2i::to_u8() const { return Vec2u8(clamp_u8(x), clamp_u8(y)); }
Vec2r Vec2i::to_r() const { return Vec2r(static_cast<float>(x), static_cast<float>(y)); }

Vec2u8 Vec2u8::operator+(const Vec2u8& o) const { return Vec2u8(clamp_u8(x + o.x), clamp_u8(y + o.y)); }
Vec2u8 Vec2u8::operator-(const Vec2u8& o) const { return Vec2u8(clamp_u8(x - o.x), clamp_u8(y - o.y)); }
Vec2u8 Vec2u8::operator*(const Vec2u8& o) const { return Vec2u8(clamp_u8(x * o.x), clamp_u8(y * o.y)); }

Vec2u8 Vec2u8::operator/(const Vec2u8& o) const {
    if (o.x == 0 || o.y == 0) throw std::domain_error("Division by zero");
    return Vec2u8(clamp_u8(x / o.x), clamp_u8(y / o.y));
}

Vec2u8 Vec2u8::operator+(uint8_t s) const { return Vec2u8(clamp_u8(x + s), clamp_u8(y + s)); }
Vec2u8 Vec2u8::operator-(uint8_t s) const { return Vec2u8(clamp_u8(x - s), clamp_u8(y - s)); }
Vec2u8 Vec2u8::operator*(uint8_t s) const { return Vec2u8(clamp_u8(x * s), clamp_u8(y * s)); }

Vec2u8 Vec2u8::operator/(uint8_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec2u8(clamp_u8(x / s), clamp_u8(y / s));
}

float Vec2u8::length() const { return std::sqrt(static_cast<float>(x * x + y * y)); }
uint8_t Vec2u8::dot(const Vec2u8& o) const { return clamp_u8(x * o.x + y * o.y); }

void Vec2u8::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = clamp_u8(static_cast<int>(std::round(x * scale)));
    y = clamp_u8(static_cast<int>(std::round(y * scale)));
}

Vec2i Vec2u8::to_i() const { return Vec2i(static_cast<int32_t>(x), static_cast<int32_t>(y)); }
Vec2f Vec2u8::to_f() const { return Vec2f(static_cast<float>(x), static_cast<float>(y)); }
Vec2r Vec2u8::to_r() const { return Vec2r(static_cast<float>(x), static_cast<float>(y)); }

Vec2f Vec2f::operator/(const Vec2f& o) const {
    if (o.x == 0.0f || o.y == 0.0f) throw std::domain_error("Division by zero");
    return Vec2f(x / o.x, y / o.y);
}

Vec2f Vec2f::operator/(float s) const {
    if (s == 0.0f) throw std::domain_error("Division by zero");
    return Vec2f(x / s, y / s);
}

float Vec2f::length() const { return std::sqrt(x * x + y * y); }

Vec2f Vec2f::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec2f(0.0f, 0.0f);
    return Vec2f(x / len, y / len);
}

void Vec2f::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    x *= (new_length / len); y *= (new_length / len);
}

Vec2f Vec2f::rotate(float angle) const {
    float cos_a = std::cos(angle), sin_a = std::sin(angle);
    return Vec2f(x * cos_a - y * sin_a, x * sin_a + y * cos_a);
}

Vec2i Vec2f::to_i() const { return Vec2i(static_cast<int32_t>(x), static_cast<int32_t>(y)); }
Vec2u8 Vec2f::to_u8() const { return Vec2u8(clamp_u8(static_cast<int>(x)), clamp_u8(static_cast<int>(y))); }
Vec2r Vec2f::to_r() const { return Vec2r(x, y); }

void Vec2r::normalize() { x = normalize_angle(x); y = normalize_angle(y); }

Vec2r Vec2r::operator+(const Vec2r& o) const { return Vec2r(x + o.x, y + o.y, auto_normalize_); }
Vec2r Vec2r::operator-(const Vec2r& o) const { return Vec2r(x - o.x, y - o.y, auto_normalize_); }
Vec2r Vec2r::operator*(const Vec2r& o) const { return Vec2r(x * o.x, y * o.y, auto_normalize_); }

Vec2r Vec2r::operator/(const Vec2r& o) const {
    if (o.x == 0.0f || o.y == 0.0f) throw std::domain_error("Division by zero");
    return Vec2r(x / o.x, y / o.y, auto_normalize_);
}
Vec2r Vec2r::operator+(float s) const { return Vec2r(x + s, y + s, auto_normalize_); }
Vec2r Vec2r::operator-(float s) const { return Vec2r(x - s, y - s, auto_normalize_); }
Vec2r Vec2r::operator*(float s) const { return Vec2r(x * s, y * s, auto_normalize_); }
Vec2r Vec2r::operator/(float s) const {
    if (s == 0.0f) throw std::domain_error("Division by zero");
    return Vec2r(x / s, y / s, auto_normalize_);
}

Vec3r Vec2r::to_vec3r() const { return Vec3r(x, y, 0.0f, auto_normalize_); }
Vec2i Vec2r::to_i() const { return Vec2i(static_cast<int32_t>(x), static_cast<int32_t>(y)); }
Vec2u8 Vec2r::to_u8() const { return Vec2u8(clamp_u8(static_cast<int>(x)), clamp_u8(static_cast<int>(y))); }
Vec2f Vec2r::to_f() const { return Vec2f(x, y); }

Vec3i Vec3i::operator/(const Vec3i& o) const {
    if (o.x == 0 || o.y == 0 || o.z == 0) throw std::domain_error("Division by zero");
    return Vec3i(x / o.x, y / o.y, z / o.z);
}

Vec3i Vec3i::operator/(int32_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec3i(x / s, y / s, z / s);
}

float Vec3i::length() const { return std::sqrt(static_cast<float>(x * x + y * y + z * z)); }

Vec3i Vec3i::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec3i(0, 0, 0);
    return Vec3i(static_cast<int32_t>(std::round(x / len)), static_cast<int32_t>(std::round(y / len)), static_cast<int32_t>(std::round(z / len)));
}

Vec3i Vec3i::cross(const Vec3i& o) const { return Vec3i(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }

void Vec3i::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = static_cast<int32_t>(std::round(x * scale)); y = static_cast<int32_t>(std::round(y * scale)); z = static_cast<int32_t>(std::round(z * scale));
}

Vec3f Vec3i::to_f() const { return Vec3f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }
Vec3u8 Vec3i::to_u8() const { return Vec3u8(clamp_u8(x), clamp_u8(y), clamp_u8(z)); }
Vec3r Vec3i::to_r() const { return Vec3r(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }

Vec3u8 Vec3u8::operator+(const Vec3u8& o) const { return Vec3u8(clamp_u8(x + o.x), clamp_u8(y + o.y), clamp_u8(z + o.z)); }
Vec3u8 Vec3u8::operator-(const Vec3u8& o) const { return Vec3u8(clamp_u8(x - o.x), clamp_u8(y - o.y), clamp_u8(z - o.z)); }
Vec3u8 Vec3u8::operator*(const Vec3u8& o) const { return Vec3u8(clamp_u8(x * o.x), clamp_u8(y * o.y), clamp_u8(z * o.z)); }

Vec3u8 Vec3u8::operator/(const Vec3u8& o) const {
    if (o.x == 0 || o.y == 0 || o.z == 0) throw std::domain_error("Division by zero");
    return Vec3u8(clamp_u8(x / o.x), clamp_u8(y / o.y), clamp_u8(z / o.z));
}

Vec3u8 Vec3u8::operator+(uint8_t s) const { return Vec3u8(clamp_u8(x + s), clamp_u8(y + s), clamp_u8(z + s)); }
Vec3u8 Vec3u8::operator-(uint8_t s) const { return Vec3u8(clamp_u8(x - s), clamp_u8(y - s), clamp_u8(z - s)); }
Vec3u8 Vec3u8::operator*(uint8_t s) const { return Vec3u8(clamp_u8(x * s), clamp_u8(y * s), clamp_u8(z * s)); }

Vec3u8 Vec3u8::operator/(uint8_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec3u8(clamp_u8(x / s), clamp_u8(y / s), clamp_u8(z / s));
}

float Vec3u8::length() const { return std::sqrt(static_cast<float>(x * x + y * y + z * z)); }
uint8_t Vec3u8::dot(const Vec3u8& o) const { return clamp_u8(x * o.x + y * o.y + z * o.z); }

Vec3u8 Vec3u8::cross(const Vec3u8& o) const { return Vec3u8(clamp_u8(y * o.z - z * o.y), clamp_u8(z * o.x - x * o.z), clamp_u8(x * o.y - y * o.x)); }

void Vec3u8::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = clamp_u8(static_cast<int>(std::round(x * scale))); y = clamp_u8(static_cast<int>(std::round(y * scale))); z = clamp_u8(static_cast<int>(std::round(z * scale)));
}

Vec3i Vec3u8::to_i() const { return Vec3i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)); }
Vec3f Vec3u8::to_f() const { return Vec3f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }
Vec3r Vec3u8::to_r() const { return Vec3r(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }

Vec3f Vec3f::operator/(const Vec3f& o) const {
    if (o.x == 0.0f || o.y == 0.0f || o.z == 0.0f) throw std::domain_error("Division by zero");
    return Vec3f(x / o.x, y / o.y, z / o.z);
}

Vec3f Vec3f::operator/(float s) const {
    if (s == 0.0f) throw std::domain_error("Division by zero");
    return Vec3f(x / s, y / s, z / s);
}

float Vec3f::length() const { return std::sqrt(x * x + y * y + z * z); }

Vec3f Vec3f::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec3f(0.0f, 0.0f, 0.0f);
    return Vec3f(x / len, y / len, z / len);
}

void Vec3f::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x *= scale; y *= scale; z *= scale;
}

Vec3f Vec3f::cross(const Vec3f& o) const { return Vec3f(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }

Vec3f Vec3f::rotate(const Vec2r& rotation) const {
    float cy = std::cos(rotation.x), sy = std::sin(rotation.x);
    float cp = std::cos(rotation.y), sp = std::sin(rotation.y);
    float x1 = x * cy - y * sy, y1 = x * sy + y * cy, z1 = z;
    return Vec3f(x1 * cp + z1 * sp, y1, -x1 * sp + z1 * cp);
}

Vec3f Vec3f::rotate(const Vec3r& rotation) const {
    float cy = std::cos(rotation.x), sy = std::sin(rotation.x);
    float cp = std::cos(rotation.y), sp = std::sin(rotation.y);
    float cr = std::cos(rotation.z), sr = std::sin(rotation.z);
    float x1 = x * cy - y * sy, y1 = x * sy + y * cy, z1 = z;
    float x2 = x1 * cp + z1 * sp, y2 = y1, z2 = -x1 * sp + z1 * cp;
    return Vec3f(x2, y2 * cr - z2 * sr, y2 * sr + z2 * cr);
}

Vec3i Vec3f::to_i() const { return Vec3i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)); }
Vec3u8 Vec3f::to_u8() const { return Vec3u8(clamp_u8(static_cast<int>(x)), clamp_u8(static_cast<int>(y)), clamp_u8(static_cast<int>(z))); }
Vec3r Vec3f::to_r() const { return Vec3r(x, y, z); }

void Vec3r::normalize() { x = normalize_angle(x); y = normalize_angle(y); z = normalize_angle(z); }

Vec3r Vec3r::operator+(const Vec3r& o) const { return Vec3r(x + o.x, y + o.y, z + o.z, auto_normalize_); }
Vec3r Vec3r::operator-(const Vec3r& o) const { return Vec3r(x - o.x, y - o.y, z - o.z, auto_normalize_); }
Vec3r Vec3r::operator*(const Vec3r& o) const { return Vec3r(x * o.x, y * o.y, z * o.z, auto_normalize_); }

Vec3r Vec3r::operator/(const Vec3r& o) const {
    if (o.x == 0.0f || o.y == 0.0f || o.z == 0.0f) throw std::domain_error("Division by zero");
    return Vec3r(x / o.x, y / o.y, z / o.z, auto_normalize_);
}

Vec3r Vec3r::operator+(float s) const { return Vec3r(x + s, y + s, z + s, auto_normalize_); }
Vec3r Vec3r::operator-(float s) const { return Vec3r(x - s, y - s, z - s, auto_normalize_); }
Vec3r Vec3r::operator*(float s) const { return Vec3r(x * s, y * s, z * s, auto_normalize_); }

Vec3r Vec3r::operator/(float s) const {
    if (s == 0.0f) throw std::domain_error("Division by zero");
    return Vec3r(x / s, y / s, z / s, auto_normalize_);
}

Vec3i Vec3r::to_i() const { return Vec3i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)); }
Vec3u8 Vec3r::to_u8() const { return Vec3u8(clamp_u8(static_cast<int>(x)), clamp_u8(static_cast<int>(y)), clamp_u8(static_cast<int>(z))); }
Vec3f Vec3r::to_f() const { return Vec3f(x, y, z); }

Vec4i Vec4i::operator/(const Vec4i& o) const {
    if (o.x == 0 || o.y == 0 || o.z == 0 || o.w == 0) throw std::domain_error("Division by zero");
    return Vec4i(x / o.x, y / o.y, z / o.z, w / o.w);
}

Vec4i Vec4i::operator/(int32_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec4i(x / s, y / s, z / s, w / s);
}

float Vec4i::length() const { return std::sqrt(static_cast<float>(x * x + y * y + z * z + w * w)); }

Vec4i Vec4i::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec4i(0, 0, 0, 0);
    return Vec4i(static_cast<int32_t>(std::round(x / len)), static_cast<int32_t>(std::round(y / len)), 
                 static_cast<int32_t>(std::round(z / len)), static_cast<int32_t>(std::round(w / len)));
}

void Vec4i::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = static_cast<int32_t>(std::round(x * scale)); y = static_cast<int32_t>(std::round(y * scale));
    z = static_cast<int32_t>(std::round(z * scale)); w = static_cast<int32_t>(std::round(w * scale));
}

Vec4f Vec4i::to_f() const { return Vec4f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)); }
Vec4u8 Vec4i::to_u8() const { return Vec4u8(clamp_u8(x), clamp_u8(y), clamp_u8(z), clamp_u8(w)); }

Vec4u8 Vec4u8::operator+(const Vec4u8& o) const { return Vec4u8(clamp_u8(x + o.x), clamp_u8(y + o.y), clamp_u8(z + o.z), clamp_u8(w + o.w)); }
Vec4u8 Vec4u8::operator-(const Vec4u8& o) const { return Vec4u8(clamp_u8(x - o.x), clamp_u8(y - o.y), clamp_u8(z - o.z), clamp_u8(w - o.w)); }
Vec4u8 Vec4u8::operator*(const Vec4u8& o) const { return Vec4u8(clamp_u8(x * o.x), clamp_u8(y * o.y), clamp_u8(z * o.z), clamp_u8(w * o.w)); }

Vec4u8 Vec4u8::operator/(const Vec4u8& o) const {
    if (o.x == 0 || o.y == 0 || o.z == 0 || o.w == 0) throw std::domain_error("Division by zero");
    return Vec4u8(clamp_u8(x / o.x), clamp_u8(y / o.y), clamp_u8(z / o.z), clamp_u8(w / o.w));
}

Vec4u8 Vec4u8::operator+(uint8_t s) const { return Vec4u8(clamp_u8(x + s), clamp_u8(y + s), clamp_u8(z + s), clamp_u8(w + s)); }
Vec4u8 Vec4u8::operator-(uint8_t s) const { return Vec4u8(clamp_u8(x - s), clamp_u8(y - s), clamp_u8(z - s), clamp_u8(w - s)); }
Vec4u8 Vec4u8::operator*(uint8_t s) const { return Vec4u8(clamp_u8(x * s), clamp_u8(y * s), clamp_u8(z * s), clamp_u8(w * s)); }

Vec4u8 Vec4u8::operator/(uint8_t s) const {
    if (s == 0) throw std::domain_error("Division by zero");
    return Vec4u8(clamp_u8(x / s), clamp_u8(y / s), clamp_u8(z / s), clamp_u8(w / s));
}

float Vec4u8::length() const { return std::sqrt(static_cast<float>(x * x + y * y + z * z + w * w)); }
uint8_t Vec4u8::dot(const Vec4u8& o) const { return clamp_u8(x * o.x + y * o.y + z * o.z + w * o.w); }

void Vec4u8::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x = clamp_u8(static_cast<int>(std::round(x * scale))); y = clamp_u8(static_cast<int>(std::round(y * scale)));
    z = clamp_u8(static_cast<int>(std::round(z * scale))); w = clamp_u8(static_cast<int>(std::round(w * scale)));
}

Vec4i Vec4u8::to_i() const { return Vec4i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z), static_cast<int32_t>(w)); }
Vec4f Vec4u8::to_f() const { return Vec4f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)); }

Vec4f Vec4f::operator/(const Vec4f& o) const {
    if (o.x == 0.0f || o.y == 0.0f || o.z == 0.0f || o.w == 0.0f) throw std::domain_error("Division by zero");
    return Vec4f(x / o.x, y / o.y, z / o.z, w / o.w);
}

Vec4f Vec4f::operator/(float s) const {
    if (s == 0.0f) throw std::domain_error("Division by zero");
    return Vec4f(x / s, y / s, z / s, w / s);
}

float Vec4f::length() const { return std::sqrt(x * x + y * y + z * z + w * w); }

Vec4f Vec4f::normalized() const {
    float len = length();
    if (len == 0.0f) return Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
    return Vec4f(x / len, y / len, z / len, w / len);
}

void Vec4f::set_length(float new_length) {
    float len = length();
    if (len == 0.0f) throw std::domain_error("Cannot set length of zero vector");
    float scale = new_length / len;
    x *= scale; y *= scale; z *= scale; w *= scale;
}

Vec4i Vec4f::to_i() const { return Vec4i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z), static_cast<int32_t>(w)); }
Vec4u8 Vec4f::to_u8() const { return Vec4u8(clamp_u8(static_cast<int>(x)), clamp_u8(static_cast<int>(y)), clamp_u8(static_cast<int>(z)), clamp_u8(static_cast<int>(w))); }


