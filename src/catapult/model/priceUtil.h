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
#define PRICE_DATA_SIZE 3

namespace catapult {
	namespace plugins {

        struct Noop {
            template <typename T>
            void operator() (T const) const noexcept {}
        };

        struct Op : Noop {
            template <typename T>
            void operator() (T const ref) const noexcept {
                delete ref;
            }
        };

        class PricePluginConfig {
            public:
                // initial supply of the network
                uint64_t initialSupply;

                // price publisher address string
                std::string pricePublisherPublicKey;

                // fee recalculation frequency
                uint64_t feeRecalculationFrequency;

                // multiplier recalculation frequency
                uint64_t multiplierRecalculationFrequency;

                // number of blocks to be included in calculating price averages (originally 30 days)
                uint64_t pricePeriodBlocks;

                // total supply and epoch fee entry life timein terms of blocks
                uint64_t entryLifetime;

                // max number of coins
                uint64_t generationCeiling;

                PricePluginConfig() = default;
        };

        class PriceDb {
            public:
                bool isDataLoaded;

                const std::string priceDirectory = "./data/price";
                std::vector<std::string> priceFields;
                std::unique_ptr<cache::RocksDatabaseSettings> priceSettings;
                std::unique_ptr<cache::RocksDatabase> handle;

                PriceDb();
        };

        class ActiveValues {
            public:
                // block height, low price, high price
                std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> priceList;
                std::deque<std::tuple<uint64_t, uint64_t, uint64_t, bool>> tempPriceList;
                
                uint64_t feeToPay;
                uint64_t collectedFees;
                uint64_t totalSupply;
                double inflationMultiplier;
                
                ActiveValues() = default;
        };

        class PriceDrivenModel {
            public:
                PricePluginConfig config;
                PriceDb priceDb;
                ActiveValues activeValues;
                ActiveValues syncActiveValues;
                bool isSync;
                std::mutex mtx;

                PriceDrivenModel() : config(), priceDb(), activeValues(), syncActiveValues() {}

                // util functions
                bool areSame(double a, double b);
                double approximate(double number);
                double getMin(double num1, double num2, double num3 = -1);

                double getCoinGenerationMultiplier(uint64_t blockHeight);
                double getMultiplier(double increase30, double increase60, double increase90);
                double getRangeAverage(uint64_t upper, uint64_t lower);
                void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
                    double &average120);
                void removeOldPrices(uint64_t blockHeight);
                void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback = false);
                void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);        
                void addPriceToDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
                void removePriceFromDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
                void commitPriceChanges();
                void addPriceFromDB(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
                void addTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
                void removeTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
                void initLoad(uint64_t blockHeight);
        };

        extern std::unique_ptr<PriceDrivenModel, Noop> priceDrivenModel;
        
        extern bool isServerProcess;
	}
}
