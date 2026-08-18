#pragma once

#include "Page.h"

class PageManager {
  public:
    void SetPage(Page *page);
    Page *GetPage() const;
    void Draw();

  private:
    Page *m_CurrentPage = nullptr;
};
