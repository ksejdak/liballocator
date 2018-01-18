////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017, Kuba Sejdak <kuba.sejdak@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////////

#include "page_allocator.h"

#include <cmath>

namespace Memory {

PageAllocator::PageAllocator()
{
    clear();
}

bool PageAllocator::init(Region* regions, std::size_t pageSize)
{
    for (std::size_t i = 0; i < regions[i].size != 0; ++i) {
        initRegionInfo(m_regionsInfo[i], regions[i], pageSize);
        ++m_validRegionsCount;
    }

    if (!countPages())
        return false;

    std::size_t descRegionIdx = chooseDescRegion();
    m_pagesHead = reinterpret_cast<Page*>(m_regionsInfo[descRegionIdx].alignedStart);
    m_pagesTail = m_pagesHead + m_pagesCount - 1;

    auto* page = m_pagesHead;
    for (std::size_t i = 0; i < m_validRegionsCount; ++i) {
        auto& region = m_regionsInfo[i];

        region.firstPage = page;
        region.lastPage = page + region.pageCount - 1;

        for (auto addr = region.alignedStart; addr != region.alignedEnd; addr += pageSize) {
            page->init();
            page->setAddress(addr);
            page = page->nextSibling();
        }

        std::size_t skipCount = (i == descRegionIdx) ? reserveDescPages() : 0;
        addGroup(region.firstPage + skipCount, region.pageCount - skipCount);
    }

    return true;
}

void PageAllocator::clear()
{
    for (auto& region : m_regionsInfo)
        clearRegionInfo(region);

    m_validRegionsCount = 0;
    m_pagesHead = nullptr;
    m_pagesTail = nullptr;
    m_freeGroupLists.fill(nullptr);
    m_pagesCount = 0;
}

std::size_t PageAllocator::countPages()
{
    for (auto& region : m_regionsInfo)
        m_pagesCount += region.pageCount;

    return m_pagesCount;
}

std::size_t PageAllocator::chooseDescRegion()
{
    std::size_t descAreaSize = m_pagesCount * sizeof(Page);

    std::size_t selectedIdx = 0;
    for (std::size_t i = 0; i < m_validRegionsCount; ++i) {
        if (m_regionsInfo[i].alignedSize < descAreaSize)
            continue;

        if (m_regionsInfo[i].alignedSize < m_regionsInfo[selectedIdx].alignedSize)
            selectedIdx = i;
    }

    return selectedIdx;
}

std::size_t PageAllocator::reserveDescPages()
{
    std::size_t reservedCount = 0;
    for (auto* page = m_pagesHead; page <= m_pagesTail; page = page->nextSibling()) {
        if (page->address() >= reinterpret_cast<uintptr_t>(m_pagesTail->nextSibling()))
            break;

        page->setUsed(true);
        ++reservedCount;
    }

    return reservedCount;
}

int PageAllocator::groupIdx(int pageCount)
{
    return static_cast<int>(ceil(log2(pageCount)) - 1);
}

void PageAllocator::addGroup(Page* group, std::size_t groupSize)
{
    Page* firstPage = group;
    Page* lastPage = group + groupSize;
    firstPage->setGroupSize(groupSize);
    lastPage->setGroupSize(groupSize);

    int idx = groupIdx(groupSize);
    group->addToList(&m_freeGroupLists[idx]);
}

void PageAllocator::removeGroup(Page* group)
{
    int idx = groupIdx(group->groupSize());
    group->removeFromList(&m_freeGroupLists[idx]);

    Page* firstPage = group;
    Page* lastPage = group + group->groupSize();
    firstPage->setGroupSize(0);
    lastPage->setGroupSize(0);
}

Page* PageAllocator::getPage(std::uintptr_t addr)
{
    RegionInfo* pageRegion = nullptr;
    for (std::size_t i = 0; i < m_validRegionsCount; ++i) {
        auto& region = m_regionsInfo[i];

        if (region.alignedStart <= addr && region.alignedEnd >= addr) {
            pageRegion = &region;
            break;
        }
    }

    if (!pageRegion)
        return nullptr;

    for (auto* page = pageRegion->firstPage; page <= pageRegion->lastPage; page = page->nextSibling()) {
        if (page->address() == addr)
            return page;
    }

    return nullptr;
}

} // namespace Memory
