#pragma once

#include <cstdint>
#include <cmath>
#include <stdexcept>


inline uint8_t clamp_u8(int value) {
    return static_cast<uint8_t>(std::max(0, std::min(255, value)));
}

class Vec2i; class Vec2u8; class Vec2f; class Vec2r;
class Vec3i; class Vec3u8; class Vec3f; class Vec3r;
class Vec4i; class Vec4u8; class Vec4f;

class Vec2i {
public:
    int32_t x, y;
    Vec2i() : x(0), y(0) {}
    Vec2i(int32_t x, int32_t y) : x(x), y(y) {}
    static Vec2i zero() { return Vec2i(0, 0); }
    static Vec2i one() { return Vec2i(1, 1); }
    static Vec2i unit_x() { return Vec2i(1, 0); }
    static Vec2i unit_y() { return Vec2i(0, 1); }
    Vec2i operator+(const Vec2i& o) const { return Vec2i(x + o.x, y + o.y); }
    Vec2i operator-(const Vec2i& o) const { return Vec2i(x - o.x, y - o.y); }
    Vec2i operator*(const Vec2i& o) const { return Vec2i(x * o.x, y * o.y); }
    Vec2i operator/(const Vec2i& o) const;
    Vec2i operator+(int32_t s) const { return Vec2i(x + s, y + s); }
    Vec2i operator-(int32_t s) const { return Vec2i(x - s, y - s); }
    Vec2i operator*(int32_t s) const { return Vec2i(x * s, y * s); }
    Vec2i operator/(int32_t s) const;
    bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2i& o) const { return !(*this == o); }
    float length() const;
    Vec2i normalized() const;
    int32_t dot(const Vec2i& o) const { return x * o.x + y * o.y; }
    void set_length(float new_length);
    Vec2f to_f() const;
    Vec2u8 to_u8() const;
    Vec2r to_r() const;
};

class Vec2u8 {
public:
    uint8_t x, y;
    Vec2u8() : x(0), y(0) {}
    Vec2u8(uint8_t x, uint8_t y) : x(x), y(y) {}
    static Vec2u8 zero() { return Vec2u8(0, 0); }
    static Vec2u8 one() { return Vec2u8(1, 1); }
    static Vec2u8 unit_x() { return Vec2u8(1, 0); }
    static Vec2u8 unit_y() { return Vec2u8(0, 1); }
    Vec2u8 operator+(const Vec2u8& o) const;
    Vec2u8 operator-(const Vec2u8& o) const;
    Vec2u8 operator*(const Vec2u8& o) const;
    Vec2u8 operator/(const Vec2u8& o) const;
    Vec2u8 operator+(uint8_t s) const;
    Vec2u8 operator-(uint8_t s) const;
    Vec2u8 operator*(uint8_t s) const;
    Vec2u8 operator/(uint8_t s) const;
    bool operator==(const Vec2u8& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2u8& o) const { return !(*this == o); }
    float length() const;
    uint8_t dot(const Vec2u8& o) const;
    void set_length(float new_length);
    Vec2i to_i() const;
    Vec2f to_f() const;
    Vec2r to_r() const;
};

class Vec2f {
public:
    float x, y;
    Vec2f() : x(0.0f), y(0.0f) {}
    Vec2f(float x, float y) : x(x), y(y) {}
    static Vec2f zero() { return Vec2f(0.0f, 0.0f); }
    static Vec2f one() { return Vec2f(1.0f, 1.0f); }
    static Vec2f unit_x() { return Vec2f(1.0f, 0.0f); }
    static Vec2f unit_y() { return Vec2f(0.0f, 1.0f); }
    Vec2f operator+(const Vec2f& o) const { return Vec2f(x + o.x, y + o.y); }
    Vec2f operator-(const Vec2f& o) const { return Vec2f(x - o.x, y - o.y); }
    Vec2f operator*(const Vec2f& o) const { return Vec2f(x * o.x, y * o.y); }
    Vec2f operator/(const Vec2f& o) const;
    Vec2f operator+(float s) const { return Vec2f(x + s, y + s); }
    Vec2f operator-(float s) const { return Vec2f(x - s, y - s); }
    Vec2f operator*(float s) const { return Vec2f(x * s, y * s); }
    Vec2f operator/(float s) const;
    bool operator==(const Vec2f& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2f& o) const { return !(*this == o); }
    float length() const;
    Vec2f normalized() const;
    float dot(const Vec2f& o) const { return x * o.x + y * o.y; }
    void set_length(float new_length);
    Vec2f rotate(float angle) const;
    Vec2i to_i() const;
    Vec2u8 to_u8() const;
    Vec2r to_r() const;
};

class Vec2r {
public:
    float x, y;
    bool auto_normalize_;
    Vec2r() : x(0.0f), y(0.0f), auto_normalize_(true) {}
    Vec2r(float x, float y, bool auto_normalize = true) : x(x), y(y), auto_normalize_(auto_normalize) {
        if (auto_normalize_) normalize();
    }
    static Vec2r zero() { return Vec2r(0.0f, 0.0f); }
    static Vec2r one() { return Vec2r(1.0f, 1.0f); }
    static Vec2r unit_x() { return Vec2r(1.0f, 0.0f); }
    static Vec2r unit_y() { return Vec2r(0.0f, 1.0f); }
    Vec2r operator+(const Vec2r& o) const;
    Vec2r operator-(const Vec2r& o) const;
    Vec2r operator*(const Vec2r& o) const;
    Vec2r operator/(const Vec2r& o) const;
    Vec2r operator+(float s) const;
    Vec2r operator-(float s) const;
    Vec2r operator*(float s) const;
    Vec2r operator/(float s) const;
    bool operator==(const Vec2r& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2r& o) const { return !(*this == o); }
    void normalize();
    Vec3r to_vec3r() const;
    Vec2i to_i() const;
    Vec2u8 to_u8() const;
    Vec2f to_f() const;
};

class Vec3i {
public:
    int32_t x, y, z;
    Vec3i() : x(0), y(0), z(0) {}
    Vec3i(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    static Vec3i zero() { return Vec3i(0, 0, 0); }
    static Vec3i one() { return Vec3i(1, 1, 1); }
    static Vec3i unit_x() { return Vec3i(1, 0, 0); }
    static Vec3i unit_y() { return Vec3i(0, 1, 0); }
    static Vec3i unit_z() { return Vec3i(0, 0, 1); }
    Vec3i operator+(const Vec3i& o) const { return Vec3i(x + o.x, y + o.y, z + o.z); }
    Vec3i operator-(const Vec3i& o) const { return Vec3i(x - o.x, y - o.y, z - o.z); }
    Vec3i operator*(const Vec3i& o) const { return Vec3i(x * o.x, y * o.y, z * o.z); }
    Vec3i operator/(const Vec3i& o) const;
    Vec3i operator+(int32_t s) const { return Vec3i(x + s, y + s, z + s); }
    Vec3i operator-(int32_t s) const { return Vec3i(x - s, y - s, z - s); }
    Vec3i operator*(int32_t s) const { return Vec3i(x * s, y * s, z * s); }
    Vec3i operator/(int32_t s) const;
    bool operator==(const Vec3i& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3i& o) const { return !(*this == o); }
    float length() const;
    Vec3i normalized() const;
    int32_t dot(const Vec3i& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3i cross(const Vec3i& o) const;
    void set_length(float new_length);
    Vec2i xy() const { return Vec2i(x, y); }
    Vec2i xz() const { return Vec2i(x, z); }
    Vec2i yz() const { return Vec2i(y, z); }
    void set_xy(const Vec2i& v) { x = v.x; y = v.y; }
    void set_xz(const Vec2i& v) { x = v.x; z = v.y; }
    void set_yz(const Vec2i& v) { y = v.x; z = v.y; }
    Vec3f to_f() const;
    Vec3u8 to_u8() const;
    Vec3r to_r() const;
};

class Vec3u8 {
public:
    uint8_t x, y, z;
    Vec3u8() : x(0), y(0), z(0) {}
    Vec3u8(uint8_t x, uint8_t y, uint8_t z) : x(x), y(y), z(z) {}
    static Vec3u8 zero() { return Vec3u8(0, 0, 0); }
    static Vec3u8 one() { return Vec3u8(1, 1, 1); }
    static Vec3u8 unit_x() { return Vec3u8(1, 0, 0); }
    static Vec3u8 unit_y() { return Vec3u8(0, 1, 0); }
    static Vec3u8 unit_z() { return Vec3u8(0, 0, 1); }
    Vec3u8 operator+(const Vec3u8& o) const;
    Vec3u8 operator-(const Vec3u8& o) const;
    Vec3u8 operator*(const Vec3u8& o) const;
    Vec3u8 operator/(const Vec3u8& o) const;
    Vec3u8 operator+(uint8_t s) const;
    Vec3u8 operator-(uint8_t s) const;
    Vec3u8 operator*(uint8_t s) const;
    Vec3u8 operator/(uint8_t s) const;
    bool operator==(const Vec3u8& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3u8& o) const { return !(*this == o); }
    float length() const;
    uint8_t dot(const Vec3u8& o) const;
    Vec3u8 cross(const Vec3u8& o) const;
    void set_length(float new_length);
    Vec2u8 xy() const { return Vec2u8(x, y); }
    Vec2u8 xz() const { return Vec2u8(x, z); }
    Vec2u8 yz() const { return Vec2u8(y, z); }
    void set_xy(const Vec2u8& v) { x = v.x; y = v.y; }
    void set_xz(const Vec2u8& v) { x = v.x; z = v.y; }
    void set_yz(const Vec2u8& v) { y = v.x; z = v.y; }
    Vec3i to_i() const;
    Vec3f to_f() const;
    Vec3r to_r() const;
};

class Vec3f {
public:
    float x, y, z;
    Vec3f() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
    static Vec3f zero() { return Vec3f(0.0f, 0.0f, 0.0f); }
    static Vec3f one() { return Vec3f(1.0f, 1.0f, 1.0f); }
    static Vec3f unit_x() { return Vec3f(1.0f, 0.0f, 0.0f); }
    static Vec3f unit_y() { return Vec3f(0.0f, 1.0f, 0.0f); }
    static Vec3f unit_z() { return Vec3f(0.0f, 0.0f, 1.0f); }
    Vec3f operator+(const Vec3f& o) const { return Vec3f(x + o.x, y + o.y, z + o.z); }
    Vec3f operator-(const Vec3f& o) const { return Vec3f(x - o.x, y - o.y, z - o.z); }
    Vec3f operator*(const Vec3f& o) const { return Vec3f(x * o.x, y * o.y, z * o.z); }
    Vec3f operator/(const Vec3f& o) const;
    Vec3f operator+(float s) const { return Vec3f(x + s, y + s, z + s); }
    Vec3f operator-(float s) const { return Vec3f(x - s, y - s, z - s); }
    Vec3f operator*(float s) const { return Vec3f(x * s, y * s, z * s); }
    Vec3f operator/(float s) const;
    bool operator==(const Vec3f& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3f& o) const { return !(*this == o); }
    float length() const;
    Vec3f normalized() const;
    float dot(const Vec3f& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3f cross(const Vec3f& o) const;
    void set_length(float new_length);
    Vec3f rotate(const Vec2r& rotation) const;
    Vec3f rotate(const Vec3r& rotation) const;
    Vec2f xy() const { return Vec2f(x, y); }
    Vec2f xz() const { return Vec2f(x, z); }
    Vec2f yz() const { return Vec2f(y, z); }
    void set_xy(const Vec2f& v) { x = v.x; y = v.y; }
    void set_xz(const Vec2f& v) { x = v.x; z = v.y; }
    void set_yz(const Vec2f& v) { y = v.x; z = v.y; }
    Vec3i to_i() const;
    Vec3u8 to_u8() const;
    Vec3r to_r() const;
};

class Vec3r {
public:
    float x, y, z;
    bool auto_normalize_;
    Vec3r() : x(0.0f), y(0.0f), z(0.0f), auto_normalize_(true) {}
    Vec3r(float x, float y, float z, bool auto_normalize = true) : x(x), y(y), z(z), auto_normalize_(auto_normalize) {
        if (auto_normalize_) normalize();
    }
    static Vec3r zero() { return Vec3r(0.0f, 0.0f, 0.0f); }
    static Vec3r one() { return Vec3r(1.0f, 1.0f, 1.0f); }
    static Vec3r unit_x() { return Vec3r(1.0f, 0.0f, 0.0f); }
    static Vec3r unit_y() { return Vec3r(0.0f, 1.0f, 0.0f); }
    static Vec3r unit_z() { return Vec3r(0.0f, 0.0f, 1.0f); }
    Vec3r operator+(const Vec3r& o) const;
    Vec3r operator-(const Vec3r& o) const;
    Vec3r operator*(const Vec3r& o) const;
    Vec3r operator/(const Vec3r& o) const;
    Vec3r operator+(float s) const;
    Vec3r operator-(float s) const;
    Vec3r operator*(float s) const;
    Vec3r operator/(float s) const;
    bool operator==(const Vec3r& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3r& o) const { return !(*this == o); }
    void normalize();
    Vec2r to_vec2r() const { return Vec2r(x, y, auto_normalize_); }
    Vec2f xy() const { return Vec2f(x, y); }
    Vec2f xz() const { return Vec2f(x, z); }
    Vec2f yz() const { return Vec2f(y, z); }
    void set_xy(const Vec2f& v) { x = v.x; y = v.y; }
    void set_xz(const Vec2f& v) { x = v.x; z = v.y; }
    void set_yz(const Vec2f& v) { y = v.x; z = v.y; }
    Vec3i to_i() const;
    Vec3u8 to_u8() const;
    Vec3f to_f() const;
};

class Vec4i {
public:
    int32_t x, y, z, w;
    Vec4i() : x(0), y(0), z(0), w(0) {}
    Vec4i(int32_t x, int32_t y, int32_t z, int32_t w) : x(x), y(y), z(z), w(w) {}
    static Vec4i zero() { return Vec4i(0, 0, 0, 0); }
    static Vec4i one() { return Vec4i(1, 1, 1, 1); }
    static Vec4i unit_x() { return Vec4i(1, 0, 0, 0); }
    static Vec4i unit_y() { return Vec4i(0, 1, 0, 0); }
    static Vec4i unit_z() { return Vec4i(0, 0, 1, 0); }
    static Vec4i unit_w() { return Vec4i(0, 0, 0, 1); }
    Vec4i operator+(const Vec4i& o) const { return Vec4i(x + o.x, y + o.y, z + o.z, w + o.w); }
    Vec4i operator-(const Vec4i& o) const { return Vec4i(x - o.x, y - o.y, z - o.z, w - o.w); }
    Vec4i operator*(const Vec4i& o) const { return Vec4i(x * o.x, y * o.y, z * o.z, w * o.w); }
    Vec4i operator/(const Vec4i& o) const;
    Vec4i operator+(int32_t s) const { return Vec4i(x + s, y + s, z + s, w + s); }
    Vec4i operator-(int32_t s) const { return Vec4i(x - s, y - s, z - s, w - s); }
    Vec4i operator*(int32_t s) const { return Vec4i(x * s, y * s, z * s, w * s); }
    Vec4i operator/(int32_t s) const;
    bool operator==(const Vec4i& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const Vec4i& o) const { return !(*this == o); }
    float length() const;
    Vec4i normalized() const;
    int32_t dot(const Vec4i& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    void set_length(float new_length);
    Vec4f to_f() const;
    Vec4u8 to_u8() const;
};

class Vec4u8 {
public:
    uint8_t x, y, z, w;
    Vec4u8() : x(0), y(0), z(0), w(0) {}
    Vec4u8(uint8_t x, uint8_t y, uint8_t z, uint8_t w) : x(x), y(y), z(z), w(w) {}
    static Vec4u8 zero() { return Vec4u8(0, 0, 0, 0); }
    static Vec4u8 one() { return Vec4u8(1, 1, 1, 1); }
    static Vec4u8 unit_x() { return Vec4u8(1, 0, 0, 0); }
    static Vec4u8 unit_y() { return Vec4u8(0, 1, 0, 0); }
    static Vec4u8 unit_z() { return Vec4u8(0, 0, 1, 0); }
    static Vec4u8 unit_w() { return Vec4u8(0, 0, 0, 1); }
    Vec4u8 operator+(const Vec4u8& o) const;
    Vec4u8 operator-(const Vec4u8& o) const;
    Vec4u8 operator*(const Vec4u8& o) const;
    Vec4u8 operator/(const Vec4u8& o) const;
    Vec4u8 operator+(uint8_t s) const;
    Vec4u8 operator-(uint8_t s) const;
    Vec4u8 operator*(uint8_t s) const;
    Vec4u8 operator/(uint8_t s) const;
    bool operator==(const Vec4u8& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const Vec4u8& o) const { return !(*this == o); }
    float length() const;
    uint8_t dot(const Vec4u8& o) const;
    void set_length(float new_length);
    Vec4i to_i() const;
    Vec4f to_f() const;
};

class Vec4f {
public:
    float x, y, z, w;
    Vec4f() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    Vec4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    static Vec4f zero() { return Vec4f(0.0f, 0.0f, 0.0f, 0.0f); }
    static Vec4f one() { return Vec4f(1.0f, 1.0f, 1.0f, 1.0f); }
    static Vec4f unit_x() { return Vec4f(1.0f, 0.0f, 0.0f, 0.0f); }
    static Vec4f unit_y() { return Vec4f(0.0f, 1.0f, 0.0f, 0.0f); }
    static Vec4f unit_z() { return Vec4f(0.0f, 0.0f, 1.0f, 0.0f); }
    static Vec4f unit_w() { return Vec4f(0.0f, 0.0f, 0.0f, 1.0f); }
    Vec4f operator+(const Vec4f& o) const { return Vec4f(x + o.x, y + o.y, z + o.z, w + o.w); }
    Vec4f operator-(const Vec4f& o) const { return Vec4f(x - o.x, y - o.y, z - o.z, w - o.w); }
    Vec4f operator*(const Vec4f& o) const { return Vec4f(x * o.x, y * o.y, z * o.z, w * o.w); }
    Vec4f operator/(const Vec4f& o) const;
    Vec4f operator+(float s) const { return Vec4f(x + s, y + s, z + s, w + s); }
    Vec4f operator-(float s) const { return Vec4f(x - s, y - s, z - s, w - s); }
    Vec4f operator*(float s) const { return Vec4f(x * s, y * s, z * s, w * s); }
    Vec4f operator/(float s) const;
    bool operator==(const Vec4f& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const Vec4f& o) const { return !(*this == o); }
    float length() const;
    Vec4f normalized() const;
    float dot(const Vec4f& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    void set_length(float new_length);
    Vec4i to_i() const;
    Vec4u8 to_u8() const;
};

// Вершина хранит только позицию и освещение.
// UV-координаты и данные текстуры вынесены в FaceData (см. mesh.h),
// что позволяет переиспользовать одни и те же вершины для разных граней.
#pragma pack(push, 1)
struct Vertex {
    float x, y, z;      // позиция
    uint8_t block_light;
    uint8_t sky_light;
    uint16_t padding;   // выравнивание до 16 байт
};
#pragma pack(pop)
static_assert(sizeof(Vertex) == 16, "Vertex must be 16 bytes");

// Данные грани: UV-координаты трёх вершин треугольника + привязка к текстуре.
// Хранится отдельно от вершин; индекс i в массиве faces соответствует
// треугольнику indices[3*i], indices[3*i+1], indices[3*i+2].
#pragma pack(push, 1)
struct FaceData {
    float u_0, u_1, u_2;
    float v_0, v_1, v_2;
    int32_t layer;      // слой в массиве текстур
    uint32_t anim_data; // для анимации достаточно 8 бит, 32 бита — подгонка до 16 байт
};
#pragma pack(pop)
static_assert(sizeof(FaceData) == 32, "FaceData must be 32 bytes");

class Transform {
public:
    Vec3f position;
    Vec3r rotation;
    Vec3f scale;
    
    Transform() : position(Vec3f::zero()), rotation(Vec3r::zero()), scale(Vec3f::one()) {}
    Transform(const Vec3f& pos, const Vec3r& rot, const Vec3f& scl) 
        : position(pos), rotation(rot), scale(scl) {}
    
    static Transform identity() { return Transform(); }
    
    Vec3f apply(const Vec3f& point) const {
        return (point * scale).rotate(rotation) + position;
    }
};


