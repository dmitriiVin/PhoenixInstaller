#pragma once

class Page {
  public:
    virtual ~Page() = default;

    virtual void Draw() = 0;

    virtual bool NextRequested() const {
        return false;
    }

    virtual void ResetState() {
    }
};