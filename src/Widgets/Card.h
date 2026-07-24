#pragma once

class Card {
  public:
    static bool Begin(const char *id, float height, bool selected = false);

    static void End();
};