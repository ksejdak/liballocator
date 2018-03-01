////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017-2018, Kuba Sejdak <kuba.sejdak@gmail.com>
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

#include "catch.hpp"
#include "utils.h"

// Make access to private members for testing.
// clang-format off
#define private     public
// clang-format on

#include <page_allocator.h>

#include <cstdlib>
#include <cstring>
#include <map>

using namespace Memory;

TEST_CASE("Page allocator is properly cleared", "[page_allocator]")
{
    PageAllocator pageAllocator;
    std::memset(&pageAllocator, 0x5a, sizeof(PageAllocator));

    pageAllocator.clear();
    for (const auto& region : pageAllocator.m_regionsInfo) {
        REQUIRE(region.start == 0);
        REQUIRE(region.end == 0);
        REQUIRE(region.alignedStart == 0);
        REQUIRE(region.alignedEnd == 0);
        REQUIRE(region.pageCount == 0);
        REQUIRE(region.size == 0);
        REQUIRE(region.alignedSize == 0);
        REQUIRE(region.firstPage == nullptr);
        REQUIRE(region.lastPage == nullptr);
    }
    REQUIRE(pageAllocator.m_validRegionsCount == 0);
    REQUIRE(pageAllocator.m_descRegionIdx == 0);
    REQUIRE(pageAllocator.m_descPagesCount == 0);
    REQUIRE(pageAllocator.m_pagesHead == nullptr);
    REQUIRE(pageAllocator.m_pagesTail == nullptr);
    for (const auto* page : pageAllocator.m_freeGroupLists)
        REQUIRE(page == nullptr);
    REQUIRE(pageAllocator.m_pagesCount == 0);
    REQUIRE(pageAllocator.m_freePagesCount == 0);
}

TEST_CASE("Pages are correctly counted", "[page_allocator]")
{
    std::size_t pageSize = 256;
    PageAllocator pageAllocator;

    SECTION("Regions: 1(1)")
    {
        std::size_t pagesCount = 1;
        auto size = pageSize * pagesCount;
        auto memory = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory.get()), size},
            {0,                            0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_pagesCount == pagesCount);
    }

    SECTION("Regions: 1(535), 2(87), 3(4)")
    {
        std::size_t pagesCount1 = 535;
        std::size_t pagesCount2 = 87;
        std::size_t pagesCount3 = 4;
        auto size1 = pageSize * pagesCount1;
        auto size2 = pageSize * pagesCount2;
        auto size3 = pageSize * pagesCount3;
        auto memory1 = test_alignedAlloc(pageSize, size1);
        auto memory2 = test_alignedAlloc(pageSize, size2);
        auto memory3 = test_alignedAlloc(pageSize, size3);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size1},
            {std::uintptr_t(memory2.get()), size2},
            {std::uintptr_t(memory3.get()), size3},
            {0,                             0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_pagesCount == (pagesCount1 + pagesCount2 + pagesCount3));
    }

    SECTION("All regions have 5 pages")
    {
        std::size_t pagesCount = 5;
        auto size = pageSize * pagesCount;
        auto memory1 = test_alignedAlloc(pageSize, size);
        auto memory2 = test_alignedAlloc(pageSize, size);
        auto memory3 = test_alignedAlloc(pageSize, size);
        auto memory4 = test_alignedAlloc(pageSize, size);
        auto memory5 = test_alignedAlloc(pageSize, size);
        auto memory6 = test_alignedAlloc(pageSize, size);
        auto memory7 = test_alignedAlloc(pageSize, size);
        auto memory8 = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size},
            {std::uintptr_t(memory2.get()), size},
            {std::uintptr_t(memory3.get()), size},
            {std::uintptr_t(memory4.get()), size},
            {std::uintptr_t(memory5.get()), size},
            {std::uintptr_t(memory6.get()), size},
            {std::uintptr_t(memory7.get()), size},
            {std::uintptr_t(memory8.get()), size},
            {0,                              0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_pagesCount == (pagesCount * 8));
    }
}

TEST_CASE("Region where page descriptors are stored is properly selected", "[page_allocator]")
{
    std::size_t pageSize = 256;
    PageAllocator pageAllocator;

    SECTION("Regions: 1(1)")
    {
        std::size_t pagesCount = 1;
        auto size = pageSize * pagesCount;
        auto memory = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory.get()), size},
            {0,                            0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descRegionIdx == 0);

    }

    SECTION("Regions: 1(535), 2(87), 3(4)")
    {
        std::size_t pagesCount1 = 535;
        std::size_t pagesCount2 = 87;
        std::size_t pagesCount3 = 4;
        auto size1 = pageSize * pagesCount1;
        auto size2 = pageSize * pagesCount2;
        auto size3 = pageSize * pagesCount3;
        auto memory1 = test_alignedAlloc(pageSize, size1);
        auto memory2 = test_alignedAlloc(pageSize, size2);
        auto memory3 = test_alignedAlloc(pageSize, size3);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size1},
            {std::uintptr_t(memory2.get()), size2},
            {std::uintptr_t(memory3.get()), size3},
            {0,                             0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descRegionIdx == 1);
    }

    SECTION("All regions have 5 pages")
    {
        std::size_t pagesCount = 5;
        auto size = pageSize * pagesCount;
        auto memory1 = test_alignedAlloc(pageSize, size);
        auto memory2 = test_alignedAlloc(pageSize, size);
        auto memory3 = test_alignedAlloc(pageSize, size);
        auto memory4 = test_alignedAlloc(pageSize, size);
        auto memory5 = test_alignedAlloc(pageSize, size);
        auto memory6 = test_alignedAlloc(pageSize, size);
        auto memory7 = test_alignedAlloc(pageSize, size);
        auto memory8 = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size},
            {std::uintptr_t(memory2.get()), size},
            {std::uintptr_t(memory3.get()), size},
            {std::uintptr_t(memory4.get()), size},
            {std::uintptr_t(memory5.get()), size},
            {std::uintptr_t(memory6.get()), size},
            {std::uintptr_t(memory7.get()), size},
            {std::uintptr_t(memory8.get()), size},
            {0,                              0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descRegionIdx == 0);
    }

    SECTION("Selected region is completly filled")
    {
        std::size_t pagesCount1 = 1;
        std::size_t pagesCount2 = 7;
        auto size1 = pageSize * pagesCount1;
        auto size2 = pageSize * pagesCount2;
        auto memory1 = test_alignedAlloc(pageSize, size1);
        auto memory2 = test_alignedAlloc(pageSize, size2);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size1},
            {std::uintptr_t(memory2.get()), size2},
            {0,                             0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descRegionIdx == 0);
    }
}

TEST_CASE("Pages with page descriptors are properly reserved", "[page_allocator]")
{
    std::size_t pageSize = 256;
    PageAllocator pageAllocator;

    SECTION("Regions: 1(1)")
    {
        std::size_t pagesCount = 1;
        auto size = pageSize * pagesCount;
        auto memory = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory.get()), size},
            {0,                            0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descPagesCount == 1);

    }

    SECTION("Regions: 1(535), 2(87), 3(4)")
    {
        std::size_t pagesCount1 = 535;
        std::size_t pagesCount2 = 87;
        std::size_t pagesCount3 = 4;
        auto size1 = pageSize * pagesCount1;
        auto size2 = pageSize * pagesCount2;
        auto size3 = pageSize * pagesCount3;
        auto memory1 = test_alignedAlloc(pageSize, size1);
        auto memory2 = test_alignedAlloc(pageSize, size2);
        auto memory3 = test_alignedAlloc(pageSize, size3);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size1},
            {std::uintptr_t(memory2.get()), size2},
            {std::uintptr_t(memory3.get()), size3},
            {0,                             0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descPagesCount == 79);
    }

    SECTION("All regions have 5 pages")
    {
        std::size_t pagesCount = 5;
        auto size = pageSize * pagesCount;
        auto memory1 = test_alignedAlloc(pageSize, size);
        auto memory2 = test_alignedAlloc(pageSize, size);
        auto memory3 = test_alignedAlloc(pageSize, size);
        auto memory4 = test_alignedAlloc(pageSize, size);
        auto memory5 = test_alignedAlloc(pageSize, size);
        auto memory6 = test_alignedAlloc(pageSize, size);
        auto memory7 = test_alignedAlloc(pageSize, size);
        auto memory8 = test_alignedAlloc(pageSize, size);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size},
            {std::uintptr_t(memory2.get()), size},
            {std::uintptr_t(memory3.get()), size},
            {std::uintptr_t(memory4.get()), size},
            {std::uintptr_t(memory5.get()), size},
            {std::uintptr_t(memory6.get()), size},
            {std::uintptr_t(memory7.get()), size},
            {std::uintptr_t(memory8.get()), size},
            {0,                              0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descPagesCount == 5);
    }

    SECTION("Selected region is completly filled")
    {
        std::size_t pagesCount1 = 1;
        std::size_t pagesCount2 = 7;
        auto size1 = pageSize * pagesCount1;
        auto size2 = pageSize * pagesCount2;
        auto memory1 = test_alignedAlloc(pageSize, size1);
        auto memory2 = test_alignedAlloc(pageSize, size2);

        // clang-format off
        Region regions[] = {
            {std::uintptr_t(memory1.get()), size1},
            {std::uintptr_t(memory2.get()), size2},
            {0,                             0}
        };
        // clang-format on

        REQUIRE(pageAllocator.init(regions, pageSize));
        REQUIRE(pageAllocator.m_descPagesCount == 1);
    }
}

TEST_CASE("Group index is properly computed", "[page_allocator]")
{
    PageAllocator pageAllocator;
    std::map<std::size_t, std::pair<std::size_t, size_t>> idxRange = {
        {0, {0, 3}},
        {1, {4, 7}},
        {2, {8, 15}},
        {3, {16, 31}},
        {4, {32, 63}},
        {5, {64, 127}},
        {6, {128, 255}},
        {7, {256, 511}},
        {8, {512, 1023}},
        {9, {1024, 2047}},
        {10, {2048, 4095}},
        {11, {4096, 8191}},
        {12, {8192, 16383}},
        {13, {16384, 32767}},
        {14, {32768, 65535}},
        {15, {65536, 131071}},
        {16, {131072, 262143}},
        {17, {262144, 524287}},
        {18, {524288, 1048575}},
        {19, {1048576, 2097151}}
    };

    for (std::size_t i = 0; i < 0x200000; ++i) {
        auto idx = pageAllocator.groupIdx(i);
        REQUIRE(i >= idxRange[idx].first);
        REQUIRE(i <= idxRange[idx].second);
    }
}

TEST_CASE("Group is properly initialized", "[page_allocator]")
{
    PageAllocator pageAllocator;

    SECTION("Group has 1 page")
    {
        constexpr std::size_t groupSize = 1;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        REQUIRE(group->groupSize() == groupSize);
    }

    SECTION("Group has 5 pages")
    {
        constexpr std::size_t groupSize = 5;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        Page* firstPage = group;
        Page* lastPage = group + groupSize - 1;
        REQUIRE(firstPage->groupSize() == groupSize);
        REQUIRE(lastPage->groupSize() == groupSize);
    }
}

TEST_CASE("Group is properly cleared", "[page_allocator]")
{
    PageAllocator pageAllocator;

    SECTION("Group has 1 page")
    {
        constexpr std::size_t groupSize = 1;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        pageAllocator.clearGroup(group);
        REQUIRE(group->groupSize() == 0);
    }

    SECTION("Group has 5 pages")
    {
        constexpr std::size_t groupSize = 5;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        pageAllocator.clearGroup(group);
        Page* firstPage = group;
        Page* lastPage = group + groupSize - 1;
        REQUIRE(firstPage->groupSize() == 0);
        REQUIRE(lastPage->groupSize() == 0);
    }
}

TEST_CASE("Group is properly added to list", "[page_allocator]")
{
    PageAllocator pageAllocator;
    std::size_t pagesCount = 0;

    SECTION("1 group is stored at index 0")
    {
        constexpr std::size_t groupSize = 3;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        pageAllocator.addGroup(group);

        for (Page* group = pageAllocator.m_freeGroupLists[0]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            ++pagesCount;
        }

        REQUIRE(pagesCount == 1);
    }

    SECTION("3 groups are stored at index 0")
    {
        constexpr std::size_t groupSize = 3;
        std::array<std::byte, sizeof(Page) * groupSize> memory1;
        std::array<std::byte, sizeof(Page) * groupSize> memory2;
        std::array<std::byte, sizeof(Page) * groupSize> memory3;
        memory1.fill(std::byte(0));
        memory2.fill(std::byte(0));
        memory3.fill(std::byte(0));

        auto* group1 = reinterpret_cast<Page*>(std::begin(memory1));
        auto* group2 = reinterpret_cast<Page*>(std::begin(memory2));
        auto* group3 = reinterpret_cast<Page*>(std::begin(memory3));
        pageAllocator.initGroup(group1, groupSize);
        pageAllocator.initGroup(group2, groupSize);
        pageAllocator.initGroup(group3, groupSize);
        pageAllocator.addGroup(group1);
        pageAllocator.addGroup(group2);
        pageAllocator.addGroup(group3);

        for (Page* group = pageAllocator.m_freeGroupLists[0]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            ++pagesCount;
        }

        REQUIRE(pagesCount == 3);
    }

    SECTION("1 group is stored at index 4")
    {
        constexpr std::size_t groupSize = 34;
        std::array<std::byte, sizeof(Page) * groupSize> memory;
        memory.fill(std::byte(0));

        auto* group = reinterpret_cast<Page*>(std::begin(memory));
        pageAllocator.initGroup(group, groupSize);
        pageAllocator.addGroup(group);

        for (Page* group = pageAllocator.m_freeGroupLists[4]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            ++pagesCount;
        }

        REQUIRE(pagesCount == 1);
    }

    SECTION("3 groups are stored at index 4")
    {
        constexpr std::size_t groupSize = 34;
        std::array<std::byte, sizeof(Page) * groupSize> memory1;
        std::array<std::byte, sizeof(Page) * groupSize> memory2;
        std::array<std::byte, sizeof(Page) * groupSize> memory3;
        memory1.fill(std::byte(0));
        memory2.fill(std::byte(0));
        memory3.fill(std::byte(0));

        auto* group1 = reinterpret_cast<Page*>(std::begin(memory1));
        auto* group2 = reinterpret_cast<Page*>(std::begin(memory2));
        auto* group3 = reinterpret_cast<Page*>(std::begin(memory3));
        pageAllocator.initGroup(group1, groupSize);
        pageAllocator.initGroup(group2, groupSize);
        pageAllocator.initGroup(group3, groupSize);
        pageAllocator.addGroup(group1);
        pageAllocator.addGroup(group2);
        pageAllocator.addGroup(group3);

        for (Page* group = pageAllocator.m_freeGroupLists[4]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            ++pagesCount;
        }

        REQUIRE(pagesCount == 3);
    }
}

TEST_CASE("Group is properly removed from list at index 0", "[page_allocator]")
{
    PageAllocator pageAllocator;
    constexpr std::size_t groupSize = 3;
    std::size_t pagesCount = 0;
    std::array<std::byte, sizeof(Page) * groupSize> memory1;
    std::array<std::byte, sizeof(Page) * groupSize> memory2;
    std::array<std::byte, sizeof(Page) * groupSize> memory3;
    memory1.fill(std::byte(0));
    memory2.fill(std::byte(0));
    memory3.fill(std::byte(0));

    auto* group1 = reinterpret_cast<Page*>(std::begin(memory1));
    auto* group2 = reinterpret_cast<Page*>(std::begin(memory2));
    auto* group3 = reinterpret_cast<Page*>(std::begin(memory3));
    pageAllocator.initGroup(group1, groupSize);
    pageAllocator.initGroup(group2, groupSize);
    pageAllocator.initGroup(group3, groupSize);
    pageAllocator.addGroup(group1);
    pageAllocator.addGroup(group2);
    pageAllocator.addGroup(group3);

    SECTION("First of three group is removed")
    {
        pageAllocator.removeGroup(group1);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[0]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group3 : group2));

            ++pagesCount;
            ++idx;
        }
    }

    SECTION("Second of three group is removed")
    {
        pageAllocator.removeGroup(group2);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[0]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group3 : group1));

            ++pagesCount;
            ++idx;
        }
    }

    SECTION("Third of three group is removed")
    {
        pageAllocator.removeGroup(group3);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[0]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group2 : group1));

            ++pagesCount;
            ++idx;
        }
    }

    REQUIRE(pagesCount == 2);
}

TEST_CASE("Group is properly removed from list at index 4", "[page_allocator]")
{
    PageAllocator pageAllocator;
    constexpr std::size_t groupSize = 34;
    std::size_t pagesCount = 0;
    std::array<std::byte, sizeof(Page) * groupSize> memory1;
    std::array<std::byte, sizeof(Page) * groupSize> memory2;
    std::array<std::byte, sizeof(Page) * groupSize> memory3;
    memory1.fill(std::byte(0));
    memory2.fill(std::byte(0));
    memory3.fill(std::byte(0));

    auto* group1 = reinterpret_cast<Page*>(std::begin(memory1));
    auto* group2 = reinterpret_cast<Page*>(std::begin(memory2));
    auto* group3 = reinterpret_cast<Page*>(std::begin(memory3));
    pageAllocator.initGroup(group1, groupSize);
    pageAllocator.initGroup(group2, groupSize);
    pageAllocator.initGroup(group3, groupSize);
    pageAllocator.addGroup(group1);
    pageAllocator.addGroup(group2);
    pageAllocator.addGroup(group3);

    SECTION("First of three group is removed")
    {
        pageAllocator.removeGroup(group1);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[4]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group3 : group2));

            ++pagesCount;
            ++idx;
        }
    }

    SECTION("Second of three group is removed")
    {
        pageAllocator.removeGroup(group2);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[4]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group3 : group1));

            ++pagesCount;
            ++idx;
        }
    }

    SECTION("Third of three group is removed")
    {
        pageAllocator.removeGroup(group3);
        int idx = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[4]; group != nullptr; group = group->nextGroup()) {
            REQUIRE(group->groupSize() == groupSize);
            REQUIRE(group == (idx == 0 ? group2 : group1));

            ++pagesCount;
            ++idx;
        }
    }

    REQUIRE(pagesCount == 2);
}

// TODO:
// - tests for splitting group
// - tests for joining groups
// - tests for resolving page from address
// - tests for allocation
// - tests for releasing
// - integration tests (long-term)
