#include "catapult/utils/Logging.h"
#include "stdint.h"
#include <deque>
#include <tuple>
#include "priceUtil.h"
#include "catapult/types.h"
#include "string.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cstdio>
#include "src/catapult/io/FileBlockStorage.h"
#include "src/catapult/model/Elements.h"

#ifdef __USE_GNU
typedef int errno_t;
#endif

namespace catapult { namespace plugins {

    std::unique_ptr<PriceDrivenModel, Noop> priceDrivenModel;
    bool isServerProcess = false;

    PriceDb::PriceDb() {
        if (isServerProcess) {
            priceFields = {"default"};
            isDataLoaded = false;
            priceSettings.reset(new cache::RocksDatabaseSettings(priceDirectory, priceFields, cache::FilterPruningMode::Disabled));
            handle.reset(new cache::RocksDatabase(*(priceSettings.get())));
        }
    }

    //region block_reward
    
    bool PriceDrivenModel::areSame(double a, double b) {
        return abs(a - b) < std::numeric_limits<double>::epsilon();
    }

    // leave up to 10 significant figures (max 5 decimal digits)
    double PriceDrivenModel::approximate(double number) {
        if (number > pow(10, 10)) {
            // if there are more than 10 digits before the decimal point, ignore the decimal digits
            number = (double)(static_cast<uint64_t>(number + 0.5));
        } else {
            for (int i = 0; i < 10; ++i) {
                if (pow(10, i + 1) > number) { // i + 1 digits left to the decimal point
                    if (i < 4)
                        i = 4;
                    number = (double)(static_cast<uint64_t>(number * pow(10, 9 - i) + 0.5)) / pow(10, 9 - i);
                    break;
                }
            }
        }
        return number;
    }

    double PriceDrivenModel::getCoinGenerationMultiplier(uint64_t blockHeight) {
        double average30 = 0, average60 = 0, average90 = 0, average120 = 0;
        getAverage(blockHeight, average30, average60, average90, average120);
        if (areSame(average60, 0)) { // either it hasn't been long enough or data is missing
            return 0;
        }
        double increase30 = average30 / average60;
        double increase60 = areSame(average90, 0) ? 0 : average60 / average90;
        double increase90 = areSame(average120, 0) ? 0 : average90 / average120;
        CATAPULT_LOG(debug) << "Increase 30: " << increase30 << ", incrase 60: " << increase60 << ", increase 90: " << increase90 << "\n";
        CATAPULT_LOG(debug) << "Get multiplier: " << getMultiplier(increase30, increase60, increase90) << "\n";
        return getMultiplier(increase30, increase60, increase90);
    }

    double PriceDrivenModel::getMultiplier(double increase30, double increase60, double increase90) {
        if (config.pricePeriodBlocks == 0) {
            return 0;
        }
        uint64_t pricePeriodsPerYear = 1051200 / config.pricePeriodBlocks; // 1051200 - number of blocks in a year
        double min;
        increase30 = approximate(increase30);
        increase60 = approximate(increase60);
        increase90 = approximate(increase90);
        if (increase30 >= 1.25 && increase60 >= 1.25) {
            if (increase90 >= 1.25) {
                min = getMin(increase30, increase60, increase90);
                if (min >= 1.55)
                    return approximate(73.5 / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.45)
                    return approximate((67 + (min - 1.45) * 6.5) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.35)
                    return approximate((61 + (min - 1.35) * 6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.25)
                    return approximate((55 + (min - 1.25) * 6) / static_cast<double>(pricePeriodsPerYear));
            } else {
                min = getMin(increase30, increase60);
                if (min >= 1.55)
                    return approximate(49 / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.45)
                    return approximate((43 + (min - 1.45) * 6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.35)
                    return approximate((37 + (min - 1.35) * 6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.25)
                    return approximate((31 + (min - 1.25) * 6) / static_cast<double>(pricePeriodsPerYear));
            }
        } else if (increase30 >= 1.05) {
            min = increase30;
            if (min >= 1.55)
                return approximate(25 / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.45)
                return approximate((19 + (min - 1.45) * 6) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.35)
                return approximate((13 + (min - 1.35) * 6) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.25)
                return approximate((9.5 + (min - 1.25) * 3.5) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.15)
                return approximate((6 + (min - 1.15) * 3.5) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.05)
                return approximate((2.5 + (min - 1.05) * 3.5) / static_cast<double>(pricePeriodsPerYear));
        }
        return 0;
    }

    double PriceDrivenModel::getRangeAverage(uint64_t upperBlock, uint64_t lowerBlock) {
        double sum = 0;
        int count = 0;
        for (auto it = activeValues.priceList.rbegin(); it != activeValues.priceList.rend(); ++it) {
            if (std::get<0>(*it) > upperBlock) {
                continue;
            } else if (std::get<0>(*it) <= lowerBlock) {
                break;
            }
            sum += static_cast<double>(std::get<1>(*it) + std::get<2>(*it));
            count++;
        }

        for (auto it = activeValues.tempPriceList.begin(); it != activeValues.tempPriceList.end(); ++it) {
            if (std::get<0>(*it) > upperBlock || std::get<0>(*it) <= lowerBlock) {
                continue;
            }
            if (std::get<3>(*it)) {
                sum += static_cast<double>(std::get<1>(*it) + std::get<2>(*it));
                count++;
            } else {
                sum -= static_cast<double>(std::get<1>(*it) + std::get<2>(*it));
                count--;
            }
        }
        
        if (count == 0) {
            return 0;
        }

        sum = sum / static_cast<double>(count) / 2.;
        approximate(sum);
        return sum;
    }

    void PriceDrivenModel::getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
        double &average120) {
        average30 = 0;
        average60 = 0;
        average90 = 0;
        average120 = 0;
        removeOldPrices(blockHeight);

        uint64_t boundary = config.pricePeriodBlocks;

        if (blockHeight >= boundary)
            average30 = getRangeAverage(blockHeight, blockHeight - boundary);

        if (blockHeight >= 2 * boundary)
            average60 = getRangeAverage(blockHeight - boundary, blockHeight - 2 * boundary);

        if (blockHeight >= 3 * boundary)
            average90 = getRangeAverage(blockHeight - 2 * boundary, blockHeight - 3 * boundary);

        if (blockHeight >= 4 * boundary)
            average120 = getRangeAverage(blockHeight - 3 * boundary, blockHeight - 4 * boundary);

        CATAPULT_LOG(debug) << "New averages found for block height " << blockHeight
            <<": 30 day average : " << average30 << ", 60 day average: " << average60
            << ", 90 day average: " << average90 << ", 120 day average: " << average120 << "\n";
    }

    double PriceDrivenModel::getMin(double num1, double num2, double num3) {
        if (areSame(num3, -1)) {
            return num1 >= num2 ? num2 : num1;
        }
        return num1 >= num2 ? (num3 >= num2 ? num2 : num3) : num1 >= num3 ? num3 : num1;
    }

    void PriceDrivenModel::processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback) {
        if (rollback) {
        	std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::reverse_iterator it;
			for (it = activeValues.priceList.rbegin(); it != activeValues.priceList.rend(); ++it) {
				if (std::get<0>(*it) == blockHeight && 
					std::get<1>(*it) == lowPrice &&
					std::get<2>(*it) == highPrice) {
					
					removeTempPrice(blockHeight, lowPrice, highPrice);
                    break;
				}
				if (std::get<0>(*it) < blockHeight) {
                    CATAPULT_LOG(error) << "ERROR: rollback price transaction not found\n";
					return; // no such price found
				}
			}
			return;
		}
		addTempPrice(blockHeight, lowPrice, highPrice);
    }
    
    //endregion block_reward

    //region price_helper

    void PriceDrivenModel::removeOldPrices(uint64_t blockHeight) {
        if (blockHeight < 4 * config.pricePeriodBlocks + config.entryLifetime) // no old blocks (store some additional blocks in case of a rollback)
            return;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = activeValues.priceList.begin(); it != activeValues.priceList.end(); ++it) {
            if (std::get<0>(*it) < blockHeight - 4 * config.pricePeriodBlocks - config.entryLifetime) { // older than 120 days + some entryLifetime blocks
                it = activeValues.priceList.erase(it);
            }
            else {
                return;
            }
            if (it == activeValues.priceList.end()) {
                break;
            }
        }
    }

    void PriceDrivenModel::removeTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        activeValues.tempPriceList.push_back({blockHeight, lowPrice, highPrice, false});
        CATAPULT_LOG(debug) << "Adding removed temp price: " << blockHeight << ", " << lowPrice << ", " << highPrice;
    }

    void PriceDrivenModel::addTempPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        if (!lowPrice || !highPrice) {
            if (!lowPrice)
                CATAPULT_LOG(error) << "Error: lowPrice is 0, must be non-zero number\n";
            if (!highPrice)
                CATAPULT_LOG(error) << "Error: highPrice is 0, must be non-zero number\n";
            return;
        } else if (lowPrice > highPrice) {
            CATAPULT_LOG(error) << "Error: highPrice can't be lower than lowPrice\n";
            return;
        }
        CATAPULT_LOG(debug) << "Adding added temp price: " << blockHeight << ", " << lowPrice << ", " << highPrice;
        activeValues.tempPriceList.push_back({blockHeight, lowPrice, highPrice, true});
    }

    void PriceDrivenModel::addPriceFromDB(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        CATAPULT_LOG(debug) << "Adding price from db: " << blockHeight << ", " << lowPrice << ", " << highPrice;
        activeValues.priceList.push_back({blockHeight, lowPrice, highPrice});
    }

    void PriceDrivenModel::addPriceToDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        removeOldPrices(blockHeight);
        activeValues.priceList.push_back({blockHeight, lowPrice, highPrice});
        addPriceEntryToFile(blockHeight, lowPrice, highPrice);

        CATAPULT_LOG(debug) << "New price added to the list for block " << blockHeight << " , lowPrice: "
            << lowPrice << ", highPrice: " << highPrice << "\n";
    }

    void PriceDrivenModel::removePriceFromDb(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        removeOldPrices(blockHeight);
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::reverse_iterator it;
        for (it = activeValues.priceList.rbegin(); it != activeValues.priceList.rend(); ++it) {
            if (blockHeight > std::get<0>(*it)) {
                CATAPULT_LOG(error) << "ERROR: price not found!";
                break;
            }
                
            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == lowPrice &&
                std::get<2>(*it) == highPrice) {
                CATAPULT_LOG(debug) << "Price removed from the list for block " << blockHeight 
                    << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << "\n";
                activeValues.priceList.erase(std::next(it).base());
                break;
            }
        }
        priceDb.handle->del(0, rocksdb::Slice(std::to_string(blockHeight)));
        priceDb.handle->flush();
    }

    void PriceDrivenModel::addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        std::string priceData[PRICE_DATA_SIZE - 1] = {
            std::to_string(lowPrice),
            std::to_string(highPrice)
        };        
        for (int i = 0; i < PRICE_DATA_SIZE - 1; ++i) {
            std::size_t priceSize = priceData[i].length();
            for (std::size_t j = 0; j < 20 - priceSize; ++j) {
                priceData[i] += ' ';
            }
        }
        for (int i = 1; i < PRICE_DATA_SIZE - 1; ++i) {
            priceData[0] += priceData[i];
        }
        priceDb.handle->put(0, rocksdb::Slice(std::to_string(blockHeight)), priceData[0]);
        priceDb.handle->flush();
    }

    void PriceDrivenModel::commitPriceChanges() {
        for (auto it = activeValues.tempPriceList.begin(); it != activeValues.tempPriceList.end(); it++) {
            if (std::get<3>(*it)) {
                addPriceToDb(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
            } else {
                // Possible to remove this step if a price for the same block is added later on
                removePriceFromDb(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
            }
        }
    }

    void PriceDrivenModel::initLoad(uint64_t blockHeight) {
        CATAPULT_LOG(debug) << "Loading initial data from db for block " << blockHeight;
        cache::RdbDataIterator result;
        std::string values[PRICE_DATA_SIZE - 1] = {""};
        blockHeight += config.entryLifetime;
        uint64_t key = blockHeight - 4 * config.pricePeriodBlocks - config.entryLifetime;
        if (key > blockHeight) {
            key = 0;
        }
        CATAPULT_LOG(debug) << "Range: " << key << " to " << blockHeight;
        while (key <= blockHeight) {
            priceDb.handle->get(0, rocksdb::Slice(std::to_string(key)), result);
            if (result.storage().empty()) {
                key++;
                continue;
            }
            values[0] = result.storage().ToString();
            values[1] = values[0].substr(20, 20);
            values[0] = values[0].substr(0, 20);

            addPriceFromDB(key, std::stoul(values[0]), std::stoul(values[1]));
            result.storage().clear();
            key++;
        }

        // Attempt to get price data for following blocks
        while (true) {
            priceDb.handle->get(0, rocksdb::Slice(std::to_string(key)), result);
            if (result.storage().empty()) {
                break;
            }
            values[0] = result.storage().ToString();
            values[1] = values[0].substr(20, 20);
            values[0] = values[0].substr(0, 20);

            addPriceFromDB(key, std::stoul(values[0]), std::stoul(values[1]));
            result.storage().clear();
            key++;
        }
    }

    //endregion price_helper
}}
