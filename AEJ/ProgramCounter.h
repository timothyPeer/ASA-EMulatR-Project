#pragma once
// ProgramCounter.h  (header-only)
#pragma once
#include <QtGlobal>

class ProgramCounter
{
  private:
    quint64 pc_;

  public:
    inline ProgramCounter() : pc_(0) {}
    inline quint64 get() const { return pc_; }
    inline void set(quint64 v) { pc_ = v & ~0x3ULL; }

    inline quint64 next() const { return pc_ + 4; }
    inline void advance() { pc_ += 4; } // always 4-byte aligned
    inline bool isAligned(quint64 v) const { return (v & 0x3) == 0; }
};
