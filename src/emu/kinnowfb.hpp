#pragma once

#include <SDL2/SDL.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bus.hpp"
#include "kinnow_palette.hpp"

enum KinnowFbRegisters : uint8_t {
  KINNOW_REG_SIZE = 0,
  KINNOW_REG_VRAM = 1,
  KINNOW_REG_STATUS = 5,
  KINNOW_REG_MODE = 6,
  KINNOW_REG_CAUSE = 7,
};

class KinnowFb : public Area {
public:
  KinnowFb(Bus &bus, int width, int height) : m_width(width), m_height(height) {
    auto self = std::shared_ptr<KinnowFb>(this, [](auto) {});

    m_framebuffer.resize(width * height * 2, 0);
    m_pixels.resize(width * height * 4, 0);

    m_slot_info[0] = 0x0c007Ca1;
    m_slot_info[1] = 0x4b494e35;

    strcpy((char *)&m_slot_info[2], "kinnowfb,16");

    m_regs[KINNOW_REG_SIZE] = height << 12 | width;
    m_regs[KINNOW_REG_VRAM] = m_framebuffer.size();

    bus.map(24, self);
  }

  // TODO: Implement double buffering/dirty regions?
  void draw(SDL_Texture *texture) {
    auto pixels = (uint32_t *)m_pixels.data();
    auto framebuffer = (uint16_t *)m_framebuffer.data();

    for (int y = 0; y < m_height; y++) {
      for (int x = 0; x < m_width; x++) {
        pixels[y * m_width + x] = kinnow_palette[framebuffer[y * m_width + x] & 0x7fff];
      }
    }

    SDL_UpdateTexture(texture, nullptr, pixels, m_width * 4);
  }

  virtual bool mem_read(uint32_t addr, BusSize size, uint32_t &value) {
    if (addr < 0x100) {
      auto slot_info = (uint8_t *)m_slot_info;
      if (size == BUS_BYTE)
        value = *(uint8_t *)&slot_info[addr];
      else if (size == BUS_INT)
        value = *(uint16_t *)&slot_info[addr];
      else if (size == BUS_LONG)
        value = *(uint32_t *)&slot_info[addr];

      return true;
    } else if (addr >= 0x3000 && addr < 0x3100) {
      addr -= 0x3000;

      auto regs = (uint8_t *)m_regs;
      if (size == BUS_BYTE)
        value = *(uint8_t *)&regs[addr];
      else if (size == BUS_INT)
        value = *(uint16_t *)&regs[addr];
      else if (size == BUS_LONG)
        value = *(uint32_t *)&regs[addr];

      return true;
    } else if (addr >= 0x100000) {
      addr -= 0x100000;
      if (addr >= m_framebuffer.size())
        return false;

      auto vram = m_framebuffer.data() + addr;
      if (size == BUS_BYTE)
        value = *(uint8_t *)vram;
      else if (size == BUS_INT)
        value = *(uint16_t *)vram;
      else if (size == BUS_LONG)
        value = *(uint32_t *)vram;

      return true;
    }

    return false;
  }

  virtual bool mem_write(uint32_t addr, BusSize size, uint32_t value) {
    if (addr >= 0x3000 && addr < 0x3100) {
      addr -= 0x3000;

      auto regs = (uint8_t *)m_regs;
      if (size == BUS_BYTE)
        *(uint8_t *)&regs[addr] = value;
      else if (size == BUS_INT)
        *(uint16_t *)&regs[addr] = value;
      else if (size == BUS_LONG)
        *(uint32_t *)&regs[addr] = value;

      return true;
    } else if (addr >= 0x100000) {
      addr -= 0x100000;
      if (addr >= m_framebuffer.size())
        return false;

      auto pixel = addr / 2;
      auto offset = pixel % m_width;
      auto line = pixel / m_height;
      auto vram = m_framebuffer.data() + addr;

      if (size == BUS_BYTE)
        *(uint8_t *)vram = value & 0xff;
      else if (size == BUS_INT)
        *(uint16_t *)vram = value & 0xffff;
      else if (size == BUS_LONG)
        *(uint32_t *)vram = value;

      return true;
    }

    return false;
  }

private:
  int m_width;
  int m_height;

  std::vector<uint8_t> m_framebuffer;
  std::vector<uint8_t> m_pixels;

  uint32_t m_slot_info[64];
  uint32_t m_regs[64];
};
