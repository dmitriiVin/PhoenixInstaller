#pragma once

class ValueStepper {
  public:
    static bool Draw(const char *id, int *value, int min, int max, float width = 240.0f, float height = 42.0f);
};