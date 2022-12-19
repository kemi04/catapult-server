#pragma once
#include <deque>
#include "stdint.h"
#include "catapult/types.h"
#include <string>
#include "catapult/cache_db/RocksDatabase.h"
#include "catapult/cache_db/RocksInclude.h"
#include <mutex>
#include "memory.h"

#ifdef __APPLE__
#define NODESTROY [[clang::no_destroy]]
#else
#define NODESTROY  
#endif

namespace catapult {
	namespace plugins {

        extern uint64_t feeToPay; // fee to pay this epoch

        // initial supply of the network
        extern uint64_t initialSupply;

        // price publisher address string
        extern std::string pricePublisherPublicKey;

        // fee recalculation frequency
        extern uint64_t feeRecalculationFrequency;

        // multiplier recalculation frequency
        extern uint64_t multiplierRecalculationFrequency;

        // number of blocks to be included in calculating price averages (originally 30 days)
        extern uint64_t pricePeriodBlocks;

        // total supply and epoch fee entry life timein terms of blocks
        extern uint64_t entryLifetime;

        extern std::unique_ptr<cache::RocksDatabase> priceDB;
        // block height, low price, high price
        extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> priceList;
        // block height, low price, high price, isAdded
        extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t, bool>> tempPriceList;

        // max number of coins
        extern uint64_t generationCeiling;
        extern uint64_t lastUpdatedBlock;
        extern const std::string priceDirectory;
        extern std::vector<std::string> priceFields;
        extern cache::RocksDatabaseSettings priceSettings;
        extern bool loaded;

        //region block_reward
        bool areSame(double a, double b);
        void configToFile();
        void readConfig();
        void readConfig(bool readOnlyDB);
        double approximate(double number);
        double getCoinGenerationMultiplier(uint64_t blockHeight);
        double getMultiplier(double increase30, double increase60, double increase90);
        void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
            double &average120);
        double getMin(double num1, double num2, double num3 = -1);

        //endregion block_reward

        //region price_helper

        void removeOldPrices(uint64_t blockHeight);
        void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback = false);
        void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
    
        void addPriceToDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void removePriceFromDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void commitPriceChanges();
        void addPriceFromDB(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void addTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void removeTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void dbCatchup();
        void initLoad(uint64_t blockHeight);
        // load prices (the upper boundary is not included)
        void loadPricesForBlockRange(uint64_t from, uint64_t to);

        //endregion price_helper
	}
}
