#pragma once
#include <deque>
#include "stdint.h"
#include "catapult/types.h"

namespace catapult {
	namespace plugins {
		extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> priceList;
        extern double currentMultiplier;
        extern uint64_t lastUpdateBlock;
        extern uint64_t epochFees;
        extern uint64_t prevEpochFees; // for rollback
        extern uint64_t feeToPay; // fee to pay this epoch
        extern uint64_t prevFeeToPay; // fee that was paid last epoch (for rollback)
        extern uint64_t totalSupply;

        void removeOldPrices(uint64_t blockHeight);
        
        void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
            double &average120);

        void addPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);

        void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);

        double getMin(double num1, double num2, double num3 = -1);

        double getCoinGenerationMultiplier(uint64_t blockHeight, bool rollback = false);

        double getMultiplier(double increase30, double increase60, double increase90);

        uint64_t getFeeToPay(uint64_t blockHeight, bool rollback = false);

        void readFromFile();

        void writeToFile();
	}
}