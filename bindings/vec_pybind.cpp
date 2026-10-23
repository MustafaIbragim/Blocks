#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include "vec.h"
#include <cmath>
#include <type_traits>
#include <string>


namespace py = pybind11;

template <typename T> struct VecTraits;

#define DEFINE_VEC_TRAITS(Type, Dim, ScalarType, ...) \
template <> struct VecTraits<Type> { \
    static constexpr int dim = Dim; \
    using scalar = ScalarType; \
    static constexpr scalar Type::* components[Dim] = { __VA_ARGS__ }; \
};

DEFINE_VEC_TRAITS(Vec2i,  2, int32_t,  &Vec2i::x,  &Vec2i::y)
DEFINE_VEC_TRAITS(Vec2u8, 2, uint8_t,  &Vec2u8::x, &Vec2u8::y)
DEFINE_VEC_TRAITS(Vec2f,  2, float,    &Vec2f::x,  &Vec2f::y)
DEFINE_VEC_TRAITS(Vec2r,  2, float,    &Vec2r::x,  &Vec2r::y)
DEFINE_VEC_TRAITS(Vec3i,  3, int32_t,  &Vec3i::x,  &Vec3i::y,  &Vec3i::z)
DEFINE_VEC_TRAITS(Vec3u8, 3, uint8_t,  &Vec3u8::x, &Vec3u8::y, &Vec3u8::z)
DEFINE_VEC_TRAITS(Vec3f,  3, float,    &Vec3f::x,  &Vec3f::y,  &Vec3f::z)
DEFINE_VEC_TRAITS(Vec3r,  3, float,    &Vec3r::x,  &Vec3r::y,  &Vec3r::z)
DEFINE_VEC_TRAITS(Vec4i,  4, int32_t,  &Vec4i::x,  &Vec4i::y,  &Vec4i::z,  &Vec4i::w)
DEFINE_VEC_TRAITS(Vec4u8, 4, uint8_t,  &Vec4u8::x, &Vec4u8::y, &Vec4u8::z, &Vec4u8::w)
DEFINE_VEC_TRAITS(Vec4f,  4, float,    &Vec4f::x,  &Vec4f::y,  &Vec4f::z,  &Vec4f::w)

template <typename TargetVec>
typename VecTraits<TargetVec>::scalar finalize_value(float val) {
    using S = typename VecTraits<TargetVec>::scalar;
    if constexpr (std::is_same_v<S, uint8_t>) {
        return clamp_u8(static_cast<int>(std::round(val)));
    } else if constexpr (std::is_integral_v<S>) {
        return static_cast<S>(std::round(val));
    } else {
        return static_cast<S>(val);
    }
}

template <typename V1, typename V2, typename Op>
V1 apply_vec_op(const V1& a, const V2& b, Op op) {
    if (VecTraits<V1>::dim != VecTraits<V2>::dim) {
        throw py::value_error("Vector dimensions must match");
    }
    
    V1 res{};
    if constexpr (std::is_same_v<V1, Vec2r> || std::is_same_v<V1, Vec3r>) {
        res.auto_normalize_ = a.auto_normalize_;
    }
    
    for (int i = 0; i < VecTraits<V1>::dim; ++i) {
        float val_a = static_cast<float>(a.*(VecTraits<V1>::components[i]));
        float val_b = static_cast<float>(b.*(VecTraits<V2>::components[i]));
        res.*(VecTraits<V1>::components[i]) = finalize_value<V1>(op(val_a, val_b));
    }
    return res;
}

template <typename V1, typename Scalar, typename Op>
V1 apply_scalar_op(const V1& a, Scalar s, Op op, bool reverse = false) {
    V1 res{};
    if constexpr (std::is_same_v<V1, Vec2r> || std::is_same_v<V1, Vec3r>) {
        res.auto_normalize_ = a.auto_normalize_;
    }
    
    float val_s = static_cast<float>(s);
    for (int i = 0; i < VecTraits<V1>::dim; ++i) {
        float val_a = static_cast<float>(a.*(VecTraits<V1>::components[i]));
        float computed = reverse ? op(val_s, val_a) : op(val_a, val_s);
        res.*(VecTraits<V1>::components[i]) = finalize_value<V1>(computed);
    }
    return res;
}

template <typename T>
void bind_vector_math(py::class_<T>& cls) {
    using Tr = VecTraits<T>;
    using Scalar = typename Tr::scalar;
    
    auto bind_universal_op = [&](const char* py_name, auto op, bool reverse = false) {
        cls.def(py_name, [op, reverse, py_name](const T& a, py::object b_obj) {
            if (py::isinstance<T>(b_obj)) {
                return apply_vec_op(a, b_obj.cast<T>(), op);
            }
            
            if (py::isinstance<Vec2i>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec2i>(), op);
            if (py::isinstance<Vec2u8>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec2u8>(), op);
            if (py::isinstance<Vec2f>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec2f>(), op);
            if (py::isinstance<Vec2r>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec2r>(), op);
            if (py::isinstance<Vec3i>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec3i>(), op);
            if (py::isinstance<Vec3u8>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec3u8>(), op);
            if (py::isinstance<Vec3f>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec3f>(), op);
            if (py::isinstance<Vec3r>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec3r>(), op);
            if (py::isinstance<Vec4i>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec4i>(), op);
            if (py::isinstance<Vec4u8>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec4u8>(), op);
            if (py::isinstance<Vec4f>(b_obj)) return apply_vec_op(a, b_obj.cast<Vec4f>(), op);
            
            if (py::isinstance<py::float_>(b_obj) || py::isinstance<py::int_>(b_obj)) {
                return apply_scalar_op(a, b_obj.cast<double>(), op, reverse);
            }
            
            throw py::type_error("Unsupported operand type for " + std::string(py_name));
        });
    };
    
    bind_universal_op("__add__",      [](float a, float b){ return a + b; });
    bind_universal_op("__radd__",     [](float a, float b){ return a + b; }, true);
    bind_universal_op("__sub__",      [](float a, float b){ return a - b; });
    bind_universal_op("__rsub__",     [](float a, float b){ return a - b; }, true);
    bind_universal_op("__mul__",      [](float a, float b){ return a * b; });
    bind_universal_op("__rmul__",     [](float a, float b){ return a * b; }, true);
    bind_universal_op("__truediv__",  [](float a, float b){ if(b==0.0f) throw std::domain_error("Division by zero"); return a / b; });
    bind_universal_op("__rtruediv__", [](float a, float b){ if(b==0.0f) throw std::domain_error("Division by zero"); return a / b; }, true);
    bind_universal_op("__pow__",      [](float a, float b){ return std::pow(a, b); });
    bind_universal_op("__rpow__",     [](float a, float b){ return std::pow(a, b); }, true);
    bind_universal_op("__mod__",      [](float a, float b){ if(b==0.0f) throw std::domain_error("Modulo by zero"); return std::fmod(a, b); });
    bind_universal_op("__rmod__",     [](float a, float b){ if(b==0.0f) throw std::domain_error("Modulo by zero"); return std::fmod(a, b); }, true);
    bind_universal_op("__floordiv__", [](float a, float b){ if(b==0.0f) throw std::domain_error("Division by zero"); return std::floor(a / b); });
    bind_universal_op("__rfloordiv__",[](float a, float b){ if(b==0.0f) throw std::domain_error("Division by zero"); return std::floor(a / b); }, true);
    
    cls.def("__eq__", [](const T& a, py::object b_obj) {
        if (py::isinstance<T>(b_obj)) {
            return a == b_obj.cast<T>();
        }
        return false;
    });
    
    cls.def("__ne__", [](const T& a, py::object b_obj) {
        if (py::isinstance<T>(b_obj)) {
            return a != b_obj.cast<T>();
        }
        return true;
    });
}

template<typename Vec3, typename Vec2>
void bind_xy_xz_yz(py::class_<Vec3>& cls,
                   Vec2 (Vec3::*get_xy)() const, Vec2 (Vec3::*get_xz)() const, Vec2 (Vec3::*get_yz)() const,
                   void (Vec3::*set_xy)(const Vec2&), void (Vec3::*set_xz)(const Vec2&), void (Vec3::*set_yz)(const Vec2&)) {
    cls.def_property("xy", get_xy, set_xy);
    cls.def_property("xz", get_xz, set_xz);
    cls.def_property("yz", get_yz, set_yz);
}

template<typename Vec>
void bind_length_property(py::class_<Vec>& cls, float (Vec::*get_length)() const, void (Vec::*set_length)(float)) {
    cls.def_property("length", get_length, set_length);
}

#define BIND_ITER_2(cls, type) \
    .def("__len__", [](const cls&) { return 2; }) \
    .def("__getitem__", [](const cls& v, int i) -> type { \
        if (i == 0) return v.x; if (i == 1) return v.y; throw py::index_error(); })

#define BIND_ITER_3(cls, type) \
    .def("__len__", [](const cls&) { return 3; }) \
    .def("__getitem__", [](const cls& v, int i) -> type { \
        if (i == 0) return v.x; if (i == 1) return v.y; if (i == 2) return v.z; throw py::index_error(); })

#define BIND_ITER_4(cls, type) \
    .def("__len__", [](const cls&) { return 4; }) \
    .def("__getitem__", [](const cls& v, int i) -> type { \
        if (i == 0) return v.x; if (i == 1) return v.y; if (i == 2) return v.z; if (i == 3) return v.w; throw py::index_error(); })


PYBIND11_MODULE(vec_module, m) {
    m.doc() = "Vector classes module";
    
    auto c_v2i = py::class_<Vec2i>(m, "Vec2i")
        .def(py::init<>()).def(py::init<int32_t, int32_t>())
        .def_readwrite("x", &Vec2i::x).def_readwrite("y", &Vec2i::y)
        .def_static("zero", &Vec2i::zero).def_static("one", &Vec2i::one).def_static("unit_x", &Vec2i::unit_x).def_static("unit_y", &Vec2i::unit_y)
        .def("normalized", &Vec2i::normalized).def("dot", &Vec2i::dot)
        .def("to_f", &Vec2i::to_f).def("to_u8", &Vec2i::to_u8).def("to_r", &Vec2i::to_r)
        BIND_ITER_2(Vec2i, int32_t)
        .def("__repr__", [](const Vec2i& v) { return "Vec2i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; });
    bind_length_property(c_v2i, &Vec2i::length, &Vec2i::set_length);
    bind_vector_math<Vec2i>(c_v2i);
    
    auto c_v2u8 = py::class_<Vec2u8>(m, "Vec2u8")
        .def(py::init<>()).def(py::init<uint8_t, uint8_t>())
        .def_readwrite("x", &Vec2u8::x).def_readwrite("y", &Vec2u8::y)
        .def_static("zero", &Vec2u8::zero).def_static("one", &Vec2u8::one).def_static("unit_x", &Vec2u8::unit_x).def_static("unit_y", &Vec2u8::unit_y)
        .def("dot", &Vec2u8::dot)
        .def("to_i", &Vec2u8::to_i).def("to_f", &Vec2u8::to_f).def("to_r", &Vec2u8::to_r)
        BIND_ITER_2(Vec2u8, uint8_t)
        .def("__repr__", [](const Vec2u8& v) { return "Vec2u8(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; });
    bind_length_property(c_v2u8, &Vec2u8::length, &Vec2u8::set_length);
    bind_vector_math<Vec2u8>(c_v2u8);
    
    auto c_v2f = py::class_<Vec2f>(m, "Vec2f")
        .def(py::init<>()).def(py::init<float, float>())
        .def_readwrite("x", &Vec2f::x).def_readwrite("y", &Vec2f::y)
        .def_static("zero", &Vec2f::zero).def_static("one", &Vec2f::one).def_static("unit_x", &Vec2f::unit_x).def_static("unit_y", &Vec2f::unit_y)
        .def("normalized", &Vec2f::normalized).def("dot", &Vec2f::dot).def("rotate", &Vec2f::rotate, py::arg("angle"))
        .def("to_i", &Vec2f::to_i).def("to_u8", &Vec2f::to_u8).def("to_r", &Vec2f::to_r)
        BIND_ITER_2(Vec2f, float)
        .def("__repr__", [](const Vec2f& v) { return "Vec2f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; });
    bind_length_property(c_v2f, &Vec2f::length, &Vec2f::set_length);
    bind_vector_math<Vec2f>(c_v2f);
    
    auto c_v2r = py::class_<Vec2r>(m, "Vec2r")
        .def(py::init<>()).def(py::init<float, float, bool>(), py::arg("x"), py::arg("y"), py::arg("auto_normalize") = true)
        .def_readwrite("x", &Vec2r::x).def_readwrite("y", &Vec2r::y).def_readwrite("auto_normalize", &Vec2r::auto_normalize_)
        .def_static("zero", &Vec2r::zero).def_static("one", &Vec2r::one).def_static("unit_x", &Vec2r::unit_x).def_static("unit_y", &Vec2r::unit_y)
        .def("normalize", &Vec2r::normalize).def("to_vec3r", &Vec2r::to_vec3r)
        .def("to_i", &Vec2r::to_i).def("to_u8", &Vec2r::to_u8).def("to_f", &Vec2r::to_f)
        BIND_ITER_2(Vec2r, float)
        .def("__repr__", [](const Vec2r& v) { return "Vec2r(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; });
    bind_vector_math<Vec2r>(c_v2r);
    
    auto c_v3i = py::class_<Vec3i>(m, "Vec3i")
        .def(py::init<>()).def(py::init<int32_t, int32_t, int32_t>())
        .def_readwrite("x", &Vec3i::x).def_readwrite("y", &Vec3i::y).def_readwrite("z", &Vec3i::z)
        .def_static("zero", &Vec3i::zero).def_static("one", &Vec3i::one).def_static("unit_x", &Vec3i::unit_x).def_static("unit_y", &Vec3i::unit_y).def_static("unit_z", &Vec3i::unit_z)
        .def("normalized", &Vec3i::normalized).def("dot", &Vec3i::dot).def("cross", &Vec3i::cross)
        .def("to_f", &Vec3i::to_f).def("to_u8", &Vec3i::to_u8).def("to_r", &Vec3i::to_r)
        BIND_ITER_3(Vec3i, int32_t)
        .def("__repr__", [](const Vec3i& v) { return "Vec3i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")"; });
    bind_length_property(c_v3i, &Vec3i::length, &Vec3i::set_length);
    bind_xy_xz_yz(c_v3i, &Vec3i::xy, &Vec3i::xz, &Vec3i::yz, &Vec3i::set_xy, &Vec3i::set_xz, &Vec3i::set_yz);
    bind_vector_math<Vec3i>(c_v3i);
    
    auto c_v3u8 = py::class_<Vec3u8>(m, "Vec3u8")
        .def(py::init<>()).def(py::init<uint8_t, uint8_t, uint8_t>())
        .def_readwrite("x", &Vec3u8::x).def_readwrite("y", &Vec3u8::y).def_readwrite("z", &Vec3u8::z)
        .def_static("zero", &Vec3u8::zero).def_static("one", &Vec3u8::one).def_static("unit_x", &Vec3u8::unit_x).def_static("unit_y", &Vec3u8::unit_y).def_static("unit_z", &Vec3u8::unit_z)
        .def("dot", &Vec3u8::dot).def("cross", &Vec3u8::cross)
        .def("to_i", &Vec3u8::to_i).def("to_f", &Vec3u8::to_f).def("to_r", &Vec3u8::to_r)
        BIND_ITER_3(Vec3u8, uint8_t)
        .def("__repr__", [](const Vec3u8& v) { return "Vec3u8(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")"; });
    bind_length_property(c_v3u8, &Vec3u8::length, &Vec3u8::set_length);
    bind_xy_xz_yz(c_v3u8, &Vec3u8::xy, &Vec3u8::xz, &Vec3u8::yz, &Vec3u8::set_xy, &Vec3u8::set_xz, &Vec3u8::set_yz);
    bind_vector_math<Vec3u8>(c_v3u8);
    
    auto c_v3f = py::class_<Vec3f>(m, "Vec3f")
        .def(py::init<>()).def(py::init<float, float, float>())
        .def_readwrite("x", &Vec3f::x).def_readwrite("y", &Vec3f::y).def_readwrite("z", &Vec3f::z)
        .def_static("zero", &Vec3f::zero).def_static("one", &Vec3f::one).def_static("unit_x", &Vec3f::unit_x).def_static("unit_y", &Vec3f::unit_y).def_static("unit_z", &Vec3f::unit_z)
        .def("normalized", &Vec3f::normalized).def("dot", &Vec3f::dot).def("cross", &Vec3f::cross)
        .def("rotate", static_cast<Vec3f (Vec3f::*)(const Vec2r&) const>(&Vec3f::rotate))
        .def("rotate", static_cast<Vec3f (Vec3f::*)(const Vec3r&) const>(&Vec3f::rotate))
        .def("to_i", &Vec3f::to_i).def("to_u8", &Vec3f::to_u8).def("to_r", &Vec3f::to_r)
        BIND_ITER_3(Vec3f, float)
        .def("__repr__", [](const Vec3f& v) { return "Vec3f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")"; });
    bind_length_property(c_v3f, &Vec3f::length, &Vec3f::set_length);
    bind_xy_xz_yz(c_v3f, &Vec3f::xy, &Vec3f::xz, &Vec3f::yz, &Vec3f::set_xy, &Vec3f::set_xz, &Vec3f::set_yz);
    bind_vector_math<Vec3f>(c_v3f);
    
    auto c_v3r = py::class_<Vec3r>(m, "Vec3r")
        .def(py::init<>()).def(py::init<float, float, float, bool>(), py::arg("x"), py::arg("y"), py::arg("z"), py::arg("auto_normalize") = true)
        .def_readwrite("x", &Vec3r::x).def_readwrite("y", &Vec3r::y).def_readwrite("z", &Vec3r::z).def_readwrite("auto_normalize", &Vec3r::auto_normalize_)
        .def_static("zero", &Vec3r::zero).def_static("one", &Vec3r::one).def_static("unit_x", &Vec3r::unit_x).def_static("unit_y", &Vec3r::unit_y).def_static("unit_z", &Vec3r::unit_z)
        .def("normalize", &Vec3r::normalize).def("to_vec2r", &Vec3r::to_vec2r)
        .def("to_i", &Vec3r::to_i).def("to_u8", &Vec3r::to_u8).def("to_f", &Vec3r::to_f)
        BIND_ITER_3(Vec3r, float)
        .def("__repr__", [](const Vec3r& v) { return "Vec3r(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")"; });
    bind_xy_xz_yz(c_v3r, &Vec3r::xy, &Vec3r::xz, &Vec3r::yz, &Vec3r::set_xy, &Vec3r::set_xz, &Vec3r::set_yz);
    bind_vector_math<Vec3r>(c_v3r);
    
    auto c_v4i = py::class_<Vec4i>(m, "Vec4i")
        .def(py::init<>()).def(py::init<int32_t, int32_t, int32_t, int32_t>())
        .def_readwrite("x", &Vec4i::x).def_readwrite("y", &Vec4i::y).def_readwrite("z", &Vec4i::z).def_readwrite("w", &Vec4i::w)
        .def_static("zero", &Vec4i::zero).def_static("one", &Vec4i::one).def_static("unit_x", &Vec4i::unit_x).def_static("unit_y", &Vec4i::unit_y).def_static("unit_z", &Vec4i::unit_z).def_static("unit_w", &Vec4i::unit_w)
        .def("normalized", &Vec4i::normalized).def("dot", &Vec4i::dot)
        .def("to_f", &Vec4i::to_f).def("to_u8", &Vec4i::to_u8)
        BIND_ITER_4(Vec4i, int32_t)
        .def("__repr__", [](const Vec4i& v) { return "Vec4i(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")"; });
    bind_length_property(c_v4i, &Vec4i::length, &Vec4i::set_length);
    bind_vector_math<Vec4i>(c_v4i);
    
    auto c_v4u8 = py::class_<Vec4u8>(m, "Vec4u8")
        .def(py::init<>()).def(py::init<uint8_t, uint8_t, uint8_t, uint8_t>())
        .def_readwrite("x", &Vec4u8::x).def_readwrite("y", &Vec4u8::y).def_readwrite("z", &Vec4u8::z).def_readwrite("w", &Vec4u8::w)
        .def_static("zero", &Vec4u8::zero).def_static("one", &Vec4u8::one).def_static("unit_x", &Vec4u8::unit_x).def_static("unit_y", &Vec4u8::unit_y).def_static("unit_z", &Vec4u8::unit_z).def_static("unit_w", &Vec4u8::unit_w)
        .def("dot", &Vec4u8::dot)
        .def("to_i", &Vec4u8::to_i).def("to_f", &Vec4u8::to_f)
        BIND_ITER_4(Vec4u8, uint8_t)
        .def("__repr__", [](const Vec4u8& v) { return "Vec4u8(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")"; });
    bind_length_property(c_v4u8, &Vec4u8::length, &Vec4u8::set_length);
    bind_vector_math<Vec4u8>(c_v4u8);
    
    auto c_v4f = py::class_<Vec4f>(m, "Vec4f")
        .def(py::init<>()).def(py::init<float, float, float, float>())
        .def_readwrite("x", &Vec4f::x).def_readwrite("y", &Vec4f::y).def_readwrite("z", &Vec4f::z).def_readwrite("w", &Vec4f::w)
        .def_static("zero", &Vec4f::zero).def_static("one", &Vec4f::one).def_static("unit_x", &Vec4f::unit_x).def_static("unit_y", &Vec4f::unit_y).def_static("unit_z", &Vec4f::unit_z).def_static("unit_w", &Vec4f::unit_w)
        .def("normalized", &Vec4f::normalized).def("dot", &Vec4f::dot)
        .def("to_i", &Vec4f::to_i).def("to_u8", &Vec4f::to_u8)
        BIND_ITER_4(Vec4f, float)
        .def("__repr__", [](const Vec4f& v) { return "Vec4f(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")"; });
    bind_length_property(c_v4f, &Vec4f::length, &Vec4f::set_length);
    bind_vector_math<Vec4f>(c_v4f);
    
    py::class_<Vertex>(m, "Vertex")
        .def(py::init<>())
        .def_readwrite("x", &Vertex::x)
        .def_readwrite("y", &Vertex::y)
        .def_readwrite("z", &Vertex::z)
        .def_readwrite("block_light", &Vertex::block_light)
        .def_readwrite("sky_light", &Vertex::sky_light)

        .def_property("position",
            [](const Vertex& v) { return Vec3f(v.x, v.y, v.z); },
            [](Vertex& v, const Vec3f& pos) { v.x = pos.x; v.y = pos.y; v.z = pos.z; }
        )

        .def("__repr__", [](const Vertex& v) {
            return "Vertex(pos=(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) +
                   "), block_light=" + std::to_string(v.block_light) +
                   ", sky_light=" + std::to_string(v.sky_light) + ")";
        });

    py::class_<FaceData>(m, "FaceData")
        .def(py::init<>())
        .def_readwrite("u_0", &FaceData::u_0)
        .def_readwrite("u_1", &FaceData::u_1)
        .def_readwrite("u_2", &FaceData::u_2)
        .def_readwrite("v_0", &FaceData::v_0)
        .def_readwrite("v_1", &FaceData::v_1)
        .def_readwrite("v_2", &FaceData::v_2)
        .def_readwrite("layer", &FaceData::layer)
        .def_readwrite("anim_data", &FaceData::anim_data)
        .def("__repr__", [](const FaceData& f) {
            return "FaceData(layer=" + std::to_string(f.layer) +
                   ", anim_data=" + std::to_string(f.anim_data) + ")";
        });

    py::class_<Transform>(m, "Transform")
        .def(py::init<>())
        .def(py::init<const Vec3f&, const Vec3r&, const Vec3f&>(), py::arg("position"), py::arg("rotation"), py::arg("scale"))
        .def_readwrite("position", &Transform::position)
        .def_readwrite("rotation", &Transform::rotation)
        .def_readwrite("scale", &Transform::scale)
        .def_static("identity", &Transform::identity)
        .def("apply", &Transform::apply, py::arg("point"), "Применяет трансформацию (scale -> rotate -> translate) к точке")
        .def("__repr__", [](const Transform& t) {
            return "Transform(pos=(" + std::to_string(t.position.x) + ", " + std::to_string(t.position.y) + ", " + std::to_string(t.position.z) + 
                   "), rot=(" + std::to_string(t.rotation.x) + ", " + std::to_string(t.rotation.y) + ", " + std::to_string(t.rotation.z) + 
                   "), scale=(" + std::to_string(t.scale.x) + ", " + std::to_string(t.scale.y) + ", " + std::to_string(t.scale.z) + "))";
        });
}


