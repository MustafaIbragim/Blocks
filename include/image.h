#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "vec.h"


class Image {
public:
    static constexpr uint32_t CHANNELS = 4;
    static constexpr uint32_t BYTES_PER_PIXEL = CHANNELS;
    
    Image(uint32_t width, uint32_t height);
    
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t channels() const { return CHANNELS; }
    uint32_t bytes_per_pixel() const { return BYTES_PER_PIXEL; }
    
    size_t row_bytes() const {
        return static_cast<size_t>(width_) * BYTES_PER_PIXEL;
    }
    size_t size_bytes() const { return data_.size(); }
    
    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    
    Vec4u8 get_pixel(uint32_t x, uint32_t y) const;
    void set_pixel(uint32_t x, uint32_t y, const Vec4u8& color);
    
    Vec4u8 get_pixel(const Vec2i& pos) const;
    void set_pixel(const Vec2i& pos, const Vec4u8& color);
    
    Vec4u8 get_pixel(const Vec2f& uv) const;
    void set_pixel(const Vec2f& uv, const Vec4u8& color);
    
    void fill(const Vec4u8& color);

private:
    size_t index(uint32_t x, uint32_t y) const;
    static uint32_t uv_to_index(float value, uint32_t size);
    
    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> data_;
};


