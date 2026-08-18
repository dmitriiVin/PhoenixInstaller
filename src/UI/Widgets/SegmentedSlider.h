#pragma once

class SegmentedSlider {
  public:
    static bool Draw(const char *id, int *value, int min, int max, float width = 0.0f);

  private:
    static int Clamp(int value, int min, int max);
};