#include "PageManager.h"

void PageManager::SetPage(Page *page) {
    m_CurrentPage = page;
}

void PageManager::Draw() {
    if (m_CurrentPage) {
        m_CurrentPage->Draw();
    }
}

Page *PageManager::GetPage() const {
    return m_CurrentPage;
}
