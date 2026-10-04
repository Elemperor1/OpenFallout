#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "apps/openfallout/mwgui/weightedsearch.hpp"

namespace OFGui
{
    namespace
    {
        TEST(OFGuiWeightedSearchTests, weightedSearchShouldNotCrashWithLargeCorpus)
        {
            EXPECT_NO_THROW(weightedSearch(std::string(100000, 'x'), std::vector<std::string>{ "x" }));
        }
        TEST(OFGuiWeightedSearchTests, weightedSearchShouldReturn1WithEmptyPatternArray)
        {
            EXPECT_EQ(weightedSearch(std::string(100, 'x'), std::vector<std::string>{}), 1);
        }
        TEST(OFGuiWeightedSearchTests, weightedSearchShouldReturnTheSumOfAllPatternsWithAtLeastOneMatch)
        {
            EXPECT_EQ(weightedSearch(std::string("xyyzzz"), std::vector<std::string>{ "x", "y", "z" }), 3);
        }
        TEST(OFGuiWeightedSearchTests, weightedSearchShouldBeCaseInsensitive)
        {
            EXPECT_EQ(weightedSearch(std::string("XYZ"), std::vector<std::string>{ "x", "y", "z" }), 3);
        }
        TEST(OFGuiWeightedSearchTests, generatePatternArrayShouldReturnEmptyArrayIfInputIsEmptyOrOnlySpaces)
        {
            EXPECT_THAT(generatePatternArray(std::string("")), testing::IsEmpty());
            EXPECT_THAT(generatePatternArray(std::string(10, ' ')), testing::IsEmpty());
        }
        TEST(OFGuiWeightedSearchTests, generatePatternArrayBasicSplittingTest)
        {
            std::vector<std::string> expected = { "x", "y", "z" };

            std::vector<std::string> output1 = generatePatternArray(std::string("x y z"));
            std::sort(output1.begin(), output1.end());

            std::vector<std::string> output2 = generatePatternArray(std::string("  x  y  z  "));
            std::sort(output2.begin(), output2.end());

            EXPECT_EQ(output1, expected);
            EXPECT_EQ(output2, expected);
        }
    }
}
