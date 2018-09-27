/////////////////////////////////////////////////////////////////////////////////////
///
/// @file
/// @author Kuba Sejdak
/// @copyright BSD 2-Clause License
///
/// Copyright (c) 2017-2018, Kuba Sejdak <kuba.sejdak@gmail.com>
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are met:
///
/// 1. Redistributions of source code must retain the above copyright notice, this
///    list of conditions and the following disclaimer.
///
/// 2. Redistributions in binary form must reproduce the above copyright notice,
///    this list of conditions and the following disclaimer in the documentation
///    and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
/// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
/// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
/// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
/// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
/// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
/// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
/// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///
/////////////////////////////////////////////////////////////////////////////////////

#include <catch2/catch.hpp>

#include <test_utils.h>

#include <chrono>
#include <cstddef>
#include <random>

// Make access to private members for testing.
// clang-format off
#define private     public
// clang-format on

#include <page_allocator.h>

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace memory;

TEST_CASE("PageAllocator integration tests (long-term)", "[integration][page_allocator]")
{
    using namespace std::chrono_literals;
    constexpr auto testDuration = 30min;
    constexpr int allocationsCount = 100;

    std::size_t pageSize = 256;
    PageAllocator pageAllocator;

    std::size_t pagesCount1 = 535;
    std::size_t pagesCount2 = 87;
    std::size_t pagesCount3 = 4;
    auto size1 = pageSize * pagesCount1;
    auto size2 = pageSize * pagesCount2;
    auto size3 = pageSize * pagesCount3;
    auto memory1 = test::alignedAlloc(pageSize, size1);
    auto memory2 = test::alignedAlloc(pageSize, size2);
    auto memory3 = test::alignedAlloc(pageSize, size3);

    // clang-format off
    Region regions[] = {
        {std::uintptr_t(memory1.get()), size1},
        {std::uintptr_t(memory2.get()), size2},
        {std::uintptr_t(memory3.get()), size3},
        {0,                             0}
    };
    // clang-format on

    REQUIRE(pageAllocator.init(regions, pageSize));
    auto freePagesCount = pageAllocator.m_freePagesCount;
    auto maxAllocSize = freePagesCount / 4;

    // Initialize random number generator.
    std::random_device randomDevice;
    std::mt19937 randomGenerator(randomDevice());
    std::uniform_int_distribution<std::size_t> distribution(0, maxAllocSize);

    std::array<Page*, allocationsCount> pages{};

    for (auto start = test::currentTime(); !test::timeElapsed(start, testDuration);) {
        pages.fill(nullptr);

        // Allocate pages.
        for (auto*& page : pages) {
            auto n = distribution(randomGenerator);
            page = pageAllocator.allocate(n);
        }

        // Release pages.
        for (auto* page : pages)
            pageAllocator.release(page);

        REQUIRE(pageAllocator.m_freePagesCount == freePagesCount);

        std::size_t idx1Count = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[1]; group != nullptr; group = group->next())
            ++idx1Count;

        REQUIRE(idx1Count == 1);
        REQUIRE(pageAllocator.m_freeGroupLists[1]->groupSize() == pagesCount3);

        std::size_t idx2Count = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[2]; group != nullptr; group = group->next())
            ++idx2Count;

        REQUIRE(idx2Count == 1);
        REQUIRE(pageAllocator.m_freeGroupLists[2]->groupSize() == pagesCount2 - pageAllocator.m_descPagesCount);

        std::size_t idx8Count = 0;
        for (Page* group = pageAllocator.m_freeGroupLists[8]; group != nullptr; group = group->next())
            ++idx8Count;

        REQUIRE(idx8Count == 1);
        REQUIRE(pageAllocator.m_freeGroupLists[8]->groupSize() == pagesCount1);

        auto stats = pageAllocator.getStats();
        REQUIRE(stats.totalMemorySize == (size1 + size2 + size3));
        REQUIRE(stats.effectiveMemorySize == (size1 + size2 + size3));
        REQUIRE(stats.userMemorySize == stats.effectiveMemorySize - (stats.pageSize * stats.reservedPagesCount));
        REQUIRE(stats.freeMemorySize == (pageSize * (stats.totalPagesCount - stats.reservedPagesCount)));
        REQUIRE(stats.pageSize == pageAllocator.m_pageSize);
        REQUIRE(stats.totalPagesCount == (pagesCount1 + pagesCount2 + pagesCount3));
        REQUIRE(stats.reservedPagesCount == 79);
        REQUIRE(stats.freePagesCount == (stats.totalPagesCount - stats.reservedPagesCount));
    }
}