#pragma once

class Card {
  public:
    static void Begin(const char *id, bool selected = false);
    static bool End();

  private:
    struct State {
        bool Selected = false;
    };

    static inline State s_State;
};