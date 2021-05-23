/**
*** Copyright (c) 2016-2019, Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp.
*** Copyright (c) 2020-present, Jaguar0625, gimre, BloodyRookie.
*** All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#include "plugins/txes/price/src/observers/priceUtil.h"
#include "tests/TestHarness.h"
#include "stdint.h"
#include <deque>
#include <tuple>
#include <cmath>
#include <fstream>

#define BLOCKS_PER_30_DAYS 86400u // number of blocks per 30 days
// epoch = 6 hours -> 4 epochs per day; number of epochs in a year: 365 * 4 = 1460
#define EPOCHS_PER_YEAR 1460
#define INCREASE_TESTS_COUNT 41
#define MOCK_PRICES_COUNT 14
#define TEST_CLASS SupplyDemandModel

namespace catapult { namespace plugins {
	namespace {
        using namespace catapult::plugins;

        double increaseTests[INCREASE_TESTS_COUNT][4] = {
            // Test 3 averages with the same growth factors

            // TEST WITH ALL INCREASES ABOVE 25%:

            /////////////////////////////////////////////////////////////////////////////
            // All increases are the same
            {1.56, 1.56, 1.56, 1 + 0.735 / EPOCHS_PER_YEAR}, //doesn't grow more than the maximum
            {1.55, 1.55, 1.55, 1 + 0.735 / EPOCHS_PER_YEAR},
            {1.50, 1.50, 1.50, 1 + 0.7025 / EPOCHS_PER_YEAR},
            {1.45, 1.45, 1.45, 1 + 0.67 / EPOCHS_PER_YEAR},
            {1.40, 1.40, 1.40, 1 + 0.64 / EPOCHS_PER_YEAR},
            {1.35, 1.35, 1.35, 1 + 0.61 / EPOCHS_PER_YEAR},
            {1.30, 1.30, 1.30, 1 + 0.58 / EPOCHS_PER_YEAR},
            {1.25, 1.25, 1.25, 1 + 0.55 / EPOCHS_PER_YEAR},

            // Test 3 averages with the 90 day increase being the smallest
            {1.55, 1.55, 1.50, 1 + 0.7025 / EPOCHS_PER_YEAR},
            {1.55, 1.45, 1.40, 1 + 0.64 / EPOCHS_PER_YEAR},
            {1.55, 1.35, 1.30, 1 + 0.58 / EPOCHS_PER_YEAR},

            // Test 3 averages with the 60 day increase being the smallest
            {1.55, 1.50, 1.55, 1 + 0.7025 / EPOCHS_PER_YEAR},
            {1.55, 1.40, 1.45, 1 + 0.64 / EPOCHS_PER_YEAR},
            {1.55, 1.30, 1.35, 1 + 0.58 / EPOCHS_PER_YEAR},

            // Test 3 averages with the 30 day increase being the smallest
            {1.5, 1.55, 1.55, 1 + 0.7025 / EPOCHS_PER_YEAR},
            {1.4, 1.45, 1.45, 1 + 0.64 / EPOCHS_PER_YEAR},
            {1.3, 1.35, 1.35, 1 + 0.58 / EPOCHS_PER_YEAR},

            /////////////////////////////////////////////////////////////////////////////

            // TEST WITH 30 AND 60 DAY INCREASES ABOVE 25%

            /////////////////////////////////////////////////////////////////////////////
            // Both incrases are the same
            {1.56, 1.56, 1, 1 + 0.49 / EPOCHS_PER_YEAR}, //doesn't grow more than the maximum
            {1.55, 1.55, 1, 1 + 0.49 / EPOCHS_PER_YEAR},
            {1.50, 1.50, 1, 1 + 0.46 / EPOCHS_PER_YEAR},
            {1.45, 1.45, 1, 1 + 0.43 / EPOCHS_PER_YEAR},
            {1.40, 1.40, 1, 1 + 0.40 / EPOCHS_PER_YEAR},
            {1.35, 1.35, 1, 1 + 0.37 / EPOCHS_PER_YEAR},
            {1.30, 1.30, 1, 1 + 0.34 / EPOCHS_PER_YEAR},
            {1.25, 1.25, 1, 1 + 0.31 / EPOCHS_PER_YEAR},

            // Test 2 averages with the 60 day increase being the smallest
            {1.55, 1.50, 1, 1 + 0.46 / EPOCHS_PER_YEAR},
            {1.55, 1.40, 1, 1 + 0.40 / EPOCHS_PER_YEAR},
            {1.55, 1.30, 1, 1 + 0.34 / EPOCHS_PER_YEAR},
            
            // Test 2 averages with the 30 day increase being the smallest
            {1.5, 1.55, 1, 1 + 0.46 / EPOCHS_PER_YEAR},
            {1.4, 1.45, 1, 1 + 0.40 / EPOCHS_PER_YEAR},
            {1.3, 1.35, 1, 1 + 0.34 / EPOCHS_PER_YEAR},
            
            /////////////////////////////////////////////////////////////////////////////

            // TEST WITH ONLY 30 DAY INCREASE
            
            /////////////////////////////////////////////////////////////////////////////

            {1.56, 1, 1, 1 + 0.25 / EPOCHS_PER_YEAR}, // Maximum reached
            {1.55, 1, 1, 1 + 0.25 / EPOCHS_PER_YEAR},
            {1.45, 1, 1, 1 + 0.19 / EPOCHS_PER_YEAR},
            {1.35, 1, 1, 1 + 0.13 / EPOCHS_PER_YEAR},
            {1.25, 1, 1, 1 + 0.095 / EPOCHS_PER_YEAR},
            {1.15, 1, 1, 1 + 0.06 / EPOCHS_PER_YEAR},
            {1.05, 1, 1, 1 + 0.025 / EPOCHS_PER_YEAR},
            {1.04, 1, 1, 1}, // Too small growth

            /////////////////////////////////////////////////////////////////////////////

            // OTHER TESTS
            
            /////////////////////////////////////////////////////////////////////////////
            // if too small, return a factor for only 30 day average
            {1.24, 1.24, 1.24, 1 + 0.0915 / EPOCHS_PER_YEAR},
            {1.55, 1.24, 1.55, 1 + 0.25 / EPOCHS_PER_YEAR},
        };

        std::tuple<uint64_t, double, double> mockPrices[MOCK_PRICES_COUNT] = {
            // Should be sorted by the blockHeight from the lowest (top) to the highest (bottom)
            // <blockHeight, lowPrice, highPrice>
            {0u, 1, 2},
            {1u, 1, 1},
            {2u, 1, 3},
            {86399u, 2, 3},
            {86400u, 3, 4},
            {86401u, 2, 4},
            {172799u, 4, 6},
            {172800u, 4, 6},
            {172801u, 2, 4},
            {259199u, 5, 7},
            {259200u, 6, 6},
            {259201u, 5, 6},
            {345599u, 4, 7},
            {345600u, 4, 7}
        };

        void generatePriceList() {
            for (long unsigned int i = 0; i < MOCK_PRICES_COUNT; ++i) {
                priceList.push_front(mockPrices[i]);
            }
        }

        void resetTests() {
            priceList.clear();
            currentMultiplier = 0;
            lastUpdateBlock = 0;
            epochFees = 0;
            feeToPay = 0;
            totalSupply = 0;
        }

        void comparePrice(std::tuple<uint64_t, uint64_t, uint64_t> price, std::tuple<uint64_t, uint64_t, uint64_t> price2) {
            EXPECT_EQ(std::get<0>(price), std::get<0>(price2));
            EXPECT_EQ(std::get<1>(price), std::get<1>(price2));
            EXPECT_EQ(std::get<2>(price), std::get<2>(price2));
        }

        double getMockPriceAverage(uint64_t end, uint64_t start = 0) {
            double average = 0;
            int count = 0;
            uint64_t blockHeight;
            for (long unsigned int i = 0; i < MOCK_PRICES_COUNT; ++i) {
                blockHeight = std::get<0>(mockPrices[i]);
                if (blockHeight > end || blockHeight < start)
                    continue;
                
                average += (std::get<1>(mockPrices[i]) + std::get<2>(mockPrices[i])) / 2;
                ++count;
            }
            return average / count;
        }

        void assertAverages(double average30, double average60, double average90, 
            double average120, uint64_t highestBlock) {
            if (highestBlock < BLOCKS_PER_30_DAYS - 1)
                return;
            EXPECT_EQ(average30, getMockPriceAverage(highestBlock,
                highestBlock - BLOCKS_PER_30_DAYS + 1u));

            if (highestBlock < BLOCKS_PER_30_DAYS * 2 - 1)
                return;
            EXPECT_EQ(average60, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS, 
                highestBlock - BLOCKS_PER_30_DAYS * 2 + 1u));
            
            if (highestBlock < BLOCKS_PER_30_DAYS * 3 - 1)
                return;
            EXPECT_EQ(average90, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS * 2, 
                highestBlock - BLOCKS_PER_30_DAYS * 3 + 1u));

            if (highestBlock < BLOCKS_PER_30_DAYS * 4 - 1)
                return;
            EXPECT_EQ(average120, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS * 3, 
                highestBlock - BLOCKS_PER_30_DAYS * 4 + 1u));
        }

        TEST(TEST_CLASS, CanRemoveOldPrices) {
            resetTests();
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it = priceList.end();
            int remainingPricesExpected = MOCK_PRICES_COUNT - 2;
            generatePriceList();
            removeOldPrices(4 * BLOCKS_PER_30_DAYS + 101); // blocks: 2 - 345601
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            for (int i = 0; i < remainingPricesExpected; ++i) {
                if (it == priceList.begin())
                    break;
                comparePrice(*--it, mockPrices[i]);
            }
	    }

        TEST(TEST_CLASS, CanGetCorrectAverages) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT; // no prices removed
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4; // blocks: 1 - 345600
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, IgnoresFuturePricesForAverages) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4 - 1u; // blocks: 0 - 345599
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan120MoreThan90Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 3; // blocks: 0 - 259200
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan90MoreThan60Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 2; // blocks: 0 - 172800
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan60MoreThan30Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS; // blocks: 0 - 86400
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan30Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = 1; // blocks: 0 - 1
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanAddPriceToPriceList) {
            resetTests();
            int remainingPricesExpected = 1;
            EXPECT_EQ(priceList.size(), 0);
            addPrice(1u, 2u, 2u);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, CantAddInvalidPriceToPriceList) {
            resetTests();
            addPrice(1u, 2u, 1u); // lowPrice can't be higher than highPrice
            addPrice(2u, 0u, 2u); // neither lowPrice nor highPrice can be 0
            addPrice(3u, 2u, 0u);
            addPrice(4u, 0u, 0u);
            EXPECT_EQ(priceList.size(), 0);
            generatePriceList();
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
            addPrice(std::get<0>(mockPrices[MOCK_PRICES_COUNT - 1]) - 1, 3u, 4u);
                // block lower than the one of an already existing price, therefore invalid
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
	    }

        TEST(TEST_CLASS, CanRemovePrice) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT - 1;
            // Remove the third price from the end
            uint64_t blockHeight = std::get<0>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t lowPrice = (uint64_t) std::get<1>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t highPrice = (uint64_t) std::get<2>(mockPrices[MOCK_PRICES_COUNT - 3]);
            generatePriceList();
            removePrice(blockHeight, lowPrice, highPrice);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, DoesNotRemoveAnythingIfPriceNotFound) {
            resetTests();
            generatePriceList();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            // Make sure such a price doesn't exist
            uint64_t blockHeight = 751u;
            uint64_t lowPrice = 696u;
            uint64_t highPrice = 697u;
            removePrice(blockHeight, lowPrice, highPrice);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, getMinChecks) {
            resetTests();
            EXPECT_EQ(getMin(1, 2), 1);
            EXPECT_EQ(getMin(2, 2), 2);
            EXPECT_EQ(getMin(3, 2, 1), 1);
            EXPECT_EQ(getMin(2, 3, 2), 2);
            EXPECT_EQ(getMin(3, 3, 3), 3);
	    }

        TEST(TEST_CLASS, getMultiplierTests) {
            resetTests();
            double multiplier;
            for (int i = 0; i < INCREASE_TESTS_COUNT; ++i) {
                multiplier = getMultiplier(increaseTests[i][0], increaseTests[i][1], increaseTests[i][2]);
                EXPECT_EQ(multiplier, increaseTests[i][3]);
            }
	    }

        // check if initial multiplier value returned (when it's initially 1) is correct
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsIndividualMultipliers) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(multiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            // namespace variable should be updated too
            EXPECT_EQ(currentMultiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3 - 1);
            EXPECT_EQ(multiplier, 1 + (0.06 + (28.0 / 23.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR);
            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 4 - 1);
            EXPECT_EQ(multiplier, 1 + (0.06 + (34.0 / 28.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR);
	    }

        // Check if the multiplier value changes according to the existing multiplier value (when it's not 1)
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsMultipleUpdates) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2);
            EXPECT_EQ(lastUpdateBlock, BLOCKS_PER_30_DAYS * 2);
            EXPECT_EQ(multiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            EXPECT_EQ(currentMultiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3);
            EXPECT_EQ(multiplier, (1 + (0.06 + (30.0 / 26.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR));
            EXPECT_EQ(currentMultiplier, (1 + (0.06 + (30.0 / 26.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR));

            lastUpdateBlock = 0;
            // Not enough blocks should reset the multiplier value to 1
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 1);
            EXPECT_EQ(multiplier, 1);
            EXPECT_EQ(currentMultiplier, 1);

            // If not yet time to update the multiplier, it shouldn't change
            currentMultiplier = 1.5;
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(currentMultiplier, 1.5);
            EXPECT_EQ(multiplier, 1.5);
	    }

        // getFeeToPay function should return the current value and not update anything
        TEST(TEST_CLASS, getFeeToPayTest_NotUpdateBlock) {
            resetTests();
            uint64_t blockHeight = 1;
            epochFees = 2;
            feeToPay = 10;
            uint64_t fee = getFeeToPay(blockHeight);
            EXPECT_EQ(fee, 10);
            EXPECT_EQ(epochFees, 2);
	    }

        // getFeeToPay function should update the feeToPay value and should reset epochFees to 0
        TEST(TEST_CLASS, getFeeToPayTest_UpdateBlock) {
            resetTests();
            uint64_t blockHeight = 720;
            epochFees = 720;
            feeToPay = 10;
            uint64_t fee = getFeeToPay(blockHeight);
            EXPECT_EQ(fee, 1);
            EXPECT_EQ(epochFees, 0);
	    }

        // check if data can be written to and read from the priceData.txt file 
        TEST(TEST_CLASS, PriceDataFileTest) {
            resetTests();
            totalSupply = 123;
            feeToPay = 234;
            epochFees = 345;
            lastUpdateBlock = 456;
            currentMultiplier = 1.23;
            addPrice(1, 2, 3);
            addPrice(4, 5, 5);
            writeToFile();
            resetTests();
            readFromFile();
            EXPECT_EQ(totalSupply, 123);
            EXPECT_EQ(feeToPay, 234);
            EXPECT_EQ(epochFees, 345);
            EXPECT_EQ(lastUpdateBlock, 456);
            EXPECT_EQ(currentMultiplier, 1.23);
            
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it = priceList.begin();
            EXPECT_EQ(std::get<0>(*it), 1);
            EXPECT_EQ(std::get<1>(*it), 2);
            EXPECT_EQ(std::get<2>(*it), 3);
            it++;

            EXPECT_EQ(std::get<0>(*it), 4);
            EXPECT_EQ(std::get<1>(*it), 5);
            EXPECT_EQ(std::get<2>(*it), 5);
	    }

        // If the priceData.txt file is empty, values shouldn't change
        TEST(TEST_CLASS, PriceDataFileTest_emptyFile) {
            resetTests();
            std::ofstream fr("priceData.txt"); // empty up the file
            readFromFile();
            EXPECT_EQ(totalSupply, 0); // values should not change
            EXPECT_EQ(feeToPay, 0);
            EXPECT_EQ(epochFees, 0);
            EXPECT_EQ(lastUpdateBlock, 0);
            EXPECT_EQ(currentMultiplier, 0);
	    }
    }
}}
