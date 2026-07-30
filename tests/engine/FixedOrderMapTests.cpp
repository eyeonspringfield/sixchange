#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "engine/FixedOrderMap.h"

namespace sixchange {
namespace {

[[nodiscard]]
std::size_t test_bucket(
    const std::uint64_t key,
    const std::size_t capacity
) noexcept
{
    std::uint64_t value = key;

    value += 0x9e3779b97f4a7c15ULL;
    value =
        (value ^ (value >> 30U)) *
        0xbf58476d1ce4e5b9ULL;
    value =
        (value ^ (value >> 27U)) *
        0x94d049bb133111ebULL;
    value ^= value >> 31U;

    return static_cast<std::size_t>(value) &
           (capacity - 1);
}

[[nodiscard]]
std::pair<std::uint64_t, std::uint64_t>
find_collision(const std::size_t capacity)
{
    for (std::uint64_t first = 1;
         first < 1'000;
         ++first) {
        for (std::uint64_t second =
                 first + 1;
             second < 1'000;
             ++second) {
            if (test_bucket(first, capacity) ==
                test_bucket(second, capacity)) {
                return {first, second};
            }
        }
    }

    throw std::runtime_error{
        "Unable to find colliding test keys"
    };
}

using Map = FixedOrderMap<std::uint64_t, int>;
using InsertResult = Map::InsertResult;

TEST(FixedOrderMapTests, RejectsZeroCapacity)
{
    EXPECT_THROW(
        Map{0},
        std::invalid_argument
    );
}

TEST(
    FixedOrderMapTests,
    RejectsNonPowerOfTwoCapacity)
{
    EXPECT_THROW(
        Map{6},
        std::invalid_argument
    );
}

TEST(
    FixedOrderMapTests,
    ReportsConfiguredCapacity)
{
    const Map map{8};

    EXPECT_EQ(map.capacity(), std::size_t{8});
    EXPECT_EQ(map.size(), std::size_t{0});
    EXPECT_TRUE(map.empty());
}

TEST(FixedOrderMapTests, InsertsAndFindsValue)
{
    Map map{8};

    EXPECT_EQ(
        map.insert(42, 123),
        InsertResult::Inserted
    );

    const auto value = map.find(42);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 123);
    EXPECT_TRUE(map.contains(42));
    EXPECT_EQ(map.size(), std::size_t{1});
}

TEST(
    FixedOrderMapTests,
    ReturnsNulloptForUnknownKey)
{
    const Map map{8};

    EXPECT_FALSE(map.find(42).has_value());
    EXPECT_FALSE(map.contains(42));
}

TEST(FixedOrderMapTests, RejectsDuplicateKey)
{
    Map map{8};

    ASSERT_EQ(
        map.insert(42, 123),
        InsertResult::Inserted
    );

    EXPECT_EQ(
        map.insert(42, 456),
        InsertResult::DuplicateKey
    );

    const auto value = map.find(42);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 123);
    EXPECT_EQ(map.size(), std::size_t{1});
}

TEST(
    FixedOrderMapTests,
    DuplicateWinsOverFullResult)
{
    Map map{2};

    ASSERT_EQ(
        map.insert(1, 10),
        InsertResult::Inserted
    );

    ASSERT_EQ(
        map.insert(2, 20),
        InsertResult::Inserted
    );

    EXPECT_EQ(
        map.insert(1, 30),
        InsertResult::DuplicateKey
    );
}

TEST(
    FixedOrderMapTests,
    ReportsFullWhenNoBucketIsAvailable)
{
    Map map{2};

    ASSERT_EQ(
        map.insert(1, 10),
        InsertResult::Inserted
    );

    ASSERT_EQ(
        map.insert(2, 20),
        InsertResult::Inserted
    );

    EXPECT_EQ(
        map.insert(3, 30),
        InsertResult::Full
    );

    EXPECT_EQ(map.size(), std::size_t{2});
}

TEST(FixedOrderMapTests, ErasesExistingKey)
{
    Map map{8};

    ASSERT_EQ(
        map.insert(42, 123),
        InsertResult::Inserted
    );

    EXPECT_TRUE(map.erase(42));
    EXPECT_FALSE(map.find(42).has_value());
    EXPECT_EQ(map.size(), std::size_t{0});
    EXPECT_TRUE(map.empty());
}

TEST(
    FixedOrderMapTests,
    RejectsErasingUnknownKey)
{
    Map map{8};

    EXPECT_FALSE(map.erase(42));
    EXPECT_EQ(map.size(), std::size_t{0});
}

TEST(
    FixedOrderMapTests,
    FindsCollisionAfterEarlierEntryIsErased)
{
    constexpr std::size_t capacity = 8;

    const auto [first_key, second_key] =
        find_collision(capacity);

    Map map{capacity};

    ASSERT_EQ(
        map.insert(first_key, 10),
        InsertResult::Inserted
    );

    ASSERT_EQ(
        map.insert(second_key, 20),
        InsertResult::Inserted
    );

    ASSERT_TRUE(map.erase(first_key));

    const auto second_value =
        map.find(second_key);

    ASSERT_TRUE(second_value.has_value());
    EXPECT_EQ(*second_value, 20);
}

TEST(
    FixedOrderMapTests,
    ReusesCapacityAfterErase)
{
    Map map{2};

    ASSERT_EQ(
        map.insert(1, 10),
        InsertResult::Inserted
    );

    ASSERT_EQ(
        map.insert(2, 20),
        InsertResult::Inserted
    );

    ASSERT_TRUE(map.erase(1));

    EXPECT_EQ(
        map.insert(3, 30),
        InsertResult::Inserted
    );

    EXPECT_TRUE(map.contains(2));
    EXPECT_TRUE(map.contains(3));
    EXPECT_EQ(map.size(), std::size_t{2});
}

TEST(
    FixedOrderMapTests,
    ErasesFromCompletelyFullTable)
{
    Map map{4};

    for (std::uint64_t key = 1;
         key <= 4;
         ++key) {
        ASSERT_EQ(
            map.insert(
                key,
                static_cast<int>(key * 10)
            ),
            InsertResult::Inserted
        );
    }

    ASSERT_TRUE(map.erase(2));

    EXPECT_FALSE(map.contains(2));
    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(3));
    EXPECT_TRUE(map.contains(4));
    EXPECT_EQ(map.size(), std::size_t{3});

    EXPECT_EQ(
        map.insert(5, 50),
        InsertResult::Inserted
    );

    EXPECT_TRUE(map.contains(5));
}

} // namespace
} // namespace sixchange