#include "image.h"

#include <limits>
#include <stdexcept>


Image::Image(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
    
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Image: width and height must be greater than 0");
    }
    
    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixels > std::numeric_limits<size_t>::max() / BYTES_PER_PIXEL) {
        throw std::length_error("Image: size overflow");
    }
    
    data_.resize(pixels * BYTES_PER_PIXEL, 0);
}

size_t Image::index(uint32_t x, uint32_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image: pixel coordinates out of bounds");
    }
    return (static_cast<size_t>(y) * width_ + x) * BYTES_PER_PIXEL;
}

Vec4u8 Image::get_pixel(uint32_t x, uint32_t y) const {
    const size_t i = index(x, y);
    return Vec4u8(data_[i], data_[i + 1], data_[i + 2], data_[i + 3]);
}

void Image::set_pixel(uint32_t x, uint32_t y, const Vec4u8& color) {
    const size_t i = index(x, y);
    data_[i]     = color.x;
    data_[i + 1] = color.y;
    data_[i + 2] = color.z;
    data_[i + 3] = color.w;
}

Vec4u8 Image::get_pixel(const Vec2i& pos) const {
    if (pos.x < 0 || pos.y < 0) {
        throw std::out_of_range("Image: negative pixel coordinates");
    }
    return get_pixel(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y));
}

void Image::set_pixel(const Vec2i& pos, const Vec4u8& color) {
    if (pos.x < 0 || pos.y < 0) {
        throw std::out_of_range("Image: negative pixel coordinates");
    }
    set_pixel(static_cast<uint32_t>(pos.x), static_cast<uint32_t>(pos.y), color);
}

uint32_t Image::uv_to_index(float value, uint32_t size) {
    if (!(value > 0.0f)) return 0;
    if (value >= 1.0f) return size - 1;
    
    const uint32_t idx = static_cast<uint32_t>(value * static_cast<float>(size));
    return idx < size ? idx : size - 1;
}

Vec4u8 Image::get_pixel(const Vec2f& uv) const {
    const uint32_t x = uv_to_index(uv.x, width_);
    const uint32_t y = uv_to_index(uv.y, height_);
    return get_pixel(x, y);
}

void Image::set_pixel(const Vec2f& uv, const Vec4u8& color) {
    const uint32_t x = uv_to_index(uv.x, width_);
    const uint32_t y = uv_to_index(uv.y, height_);
    set_pixel(x, y, color);
}

void Image::fill(const Vec4u8& color) {
    for (size_t i = 0; i < data_.size(); i += BYTES_PER_PIXEL) {
        data_[i]     = color.x;
        data_[i + 1] = color.y;
        data_[i + 2] = color.z;
        data_[i + 3] = color.w;
    }
}


