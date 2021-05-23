#include "catapult/utils/Logging.h"
#include "stdint.h"
#include <deque>
#include <tuple>
#include "priceUtil.h"
#include "catapult/types.h"
#include <fstream>
#include <filesystem>

// epoch = 6 hours -> 4 epochs per day; number of epochs in a year: 365 * 4 = 1460
#define EPOCHS_PER_YEAR 1460

namespace catapult { namespace plugins {
    std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> priceList;
    double currentMultiplier = 1;
    uint64_t lastUpdateBlock = 0;
    uint64_t epochFees = 0;
    uint64_t prevEpochFees = 0;
    uint64_t feeToPay = 0;
    uint64_t prevFeeToPay = 0;
    uint64_t totalSupply = 10000000000;


    void removeOldPrices(uint64_t blockHeight) {
        if (blockHeight < 345600u + 100u) // no old blocks (store additional 100 blocks in case of a rollback)
            return;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = --priceList.end(); it != priceList.begin(); --it) {
            if (std::get<0>(*it) < blockHeight - 345599u - 100u) { // older than 120 days 
                priceList.erase(it);
            }
            else
                return;
        }
        if (std::get<0>(*priceList.begin()) < blockHeight - 345599u - 100u)
            priceList.erase(it);
    }

    void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
        double &average120) {
        average30 = 0;
        average60 = 0;
        average90 = 0;
        average120 = 0;
        removeOldPrices(blockHeight);
        int count = 0;
        uint64_t boundary = 86400; // number of blocks equivalent to 30 days
        double *averagePtr = &average30;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            if (std::get<0>(*it) < blockHeight + 1u - boundary) {
                if (averagePtr == &average30) {
                    averagePtr = &average60;
                    if (count > 0)
                        average30 = average30 / count / 2;
                } else if (averagePtr == &average60) {
                    averagePtr = &average90;
                    if (count > 0)
                        average60 = average60 / count / 2;
                } else if (averagePtr == &average90) {
                    averagePtr = &average120;
                    if (count > 0)
                        average90 = average90 / count / 2;
                } else {
                    break; // 120 days reached
                }
                count = 0;
                boundary += 86400;
                if (blockHeight + 1u < boundary) // not enough blocks for the next 30 days
                    break;
            } else if (std::get<0>(*it) > blockHeight) {
                // ignore price messages into the future
                continue;
            }
            *averagePtr += (double)(std::get<1>(*it) + std::get<2>(*it));
            ++count;
        }
        if (count > 0 && blockHeight + 1u >= boundary)
            *averagePtr = *averagePtr / count / 2;
        else
            *averagePtr = 0;

        CATAPULT_LOG(debug) << "New averages found for block height " << blockHeight
            <<": 30 day average : " << average30 << ", 60 day average: " << average60
            << ", 90 day average: " << average90 << ", 120 day average: " << average120 << "\n";
    }

    void addPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        removeOldPrices(blockHeight);

        // must be non-zero
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
        uint64_t previousTransactionHeight;

        if (priceList.size() > 0) {
            previousTransactionHeight = std::get<0>(priceList.front());
            if (previousTransactionHeight > blockHeight) {
                CATAPULT_LOG(error) << "Error: price transaction block height is lower than the previous\n";
                return;
            }

            if (previousTransactionHeight == blockHeight) {
                CATAPULT_LOG(warning) << "Warning: price transaction block height is equal to the previous"
                    << " (potentially duplicate transaction)\n";
            }
        }
        priceList.push_front({blockHeight, lowPrice, highPrice});
        writeToFile(); // update data in the file

        CATAPULT_LOG(debug) << "New price added to the list for block " << blockHeight
            << " , lowPrice: " << lowPrice << ", highPrice: " << highPrice << "\n";
    }

    void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice) {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == lowPrice &&
                std::get<2>(*it) == highPrice) {
                it = priceList.erase(it);
                CATAPULT_LOG(debug) << "Price removed from the list for block " << blockHeight 
                    << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << "\n";
                if (it == priceList.end())
                    break;
            }
            if (blockHeight > std::get<0>(*it))
                break;
        }
        writeToFile(); // update data in the file
    }

    double getMin(double num1, double num2, double num3) {
        if (num3 == -1) {
            return num1 >= num2 ? num2 : num1;
        }
        return num1 >= num2 ? (num3 >= num2 ? num2 : num3) : num1; 
    }

    double getMultiplier(double increase30, double increase60, double increase90) {
        double min;
        if (increase30 >= 1.25 && increase60 >= 1.25) {
            if (increase90 >= 1.25) {
                min = getMin(increase30, increase60, increase90);
                if (min >= 1.55)
                    return 1 + 0.735 / EPOCHS_PER_YEAR;
                else if (min >= 1.45)
                    return 1 + (0.67 + (min - 1.45) * 0.65) / EPOCHS_PER_YEAR;
                else if (min >= 1.35)
                    return 1 + (0.61 + (min - 1.35) * 0.6) / EPOCHS_PER_YEAR;
                else if (min >= 1.25)
                    return 1 + (0.55 + (min - 1.25) * 0.6) / EPOCHS_PER_YEAR;
            } else {
                min = getMin(increase30, increase60);
                if (min >= 1.55)
                    return 1 + 0.49 / EPOCHS_PER_YEAR;
                else if (min >= 1.45)
                    return 1 + (0.43 + (min - 1.45) * 0.6) / EPOCHS_PER_YEAR;
                else if (min >= 1.35)
                    return 1 + (0.37 + (min - 1.35) * 0.6) / EPOCHS_PER_YEAR;
                else if (min >= 1.25)
                    return 1 + (0.31 + (min - 1.25) * 0.6) / EPOCHS_PER_YEAR;
            }
        } else if (increase30 >= 1.05) {
            min = increase30;
            if (min >= 1.55)
                return 1 + 0.25 / EPOCHS_PER_YEAR;
            else if (min >= 1.45)
                return 1 + (0.19 + (min - 1.45) * 0.6) / EPOCHS_PER_YEAR;
            else if (min >= 1.35)
                return 1 + (0.13 + (min - 1.35) * 0.6) / EPOCHS_PER_YEAR;
            else if (min >= 1.25)
                return 1 + (0.095 + (min - 1.25) * 0.35) / EPOCHS_PER_YEAR;
            else if (min >= 1.15)
                return 1 + (0.06 + (min - 1.15) * 0.35) / EPOCHS_PER_YEAR;
            else if (min >= 1.05)
                return 1 + (0.025 + (min - 1.05) * 0.35) / EPOCHS_PER_YEAR;
        }
        return 1;
    }

    double getCoinGenerationMultiplier(uint64_t blockHeight, bool rollback) {
        if (blockHeight % 720 > 0 && currentMultiplier != 0 && !rollback) // recalculate only every 720 blocks
            return currentMultiplier;
        else if (lastUpdateBlock >= blockHeight && !rollback)
            return currentMultiplier;
        else if (currentMultiplier == 0) // for testing purposes only
            currentMultiplier = 1;

        if (!rollback) {
            lastUpdateBlock = blockHeight;
        }
        double average30 = 0, average60 = 0, average90 = 0, average120 = 0;
        getAverage(blockHeight, average30, average60, average90, average120);
        if (average60 == 0) { // either it hasn't been long enough or data is missing
            currentMultiplier = 1;
            writeToFile(); // update data in the file
            return 1;
        }
        double increase30 = (double)average30 / (double)average60;
        double increase60 = average90 == 0 ? 0 : (double)average60 / (double)average90;
        double increase90 = average120 == 0 ? 0 : (double)average90 / (double)average120;
        currentMultiplier = currentMultiplier * getMultiplier(increase30, increase60, increase90);
        writeToFile(); // update data in the file
        return currentMultiplier;
    }

    uint64_t getFeeToPay(uint64_t blockHeight, bool rollback) {
        if (rollback && (uint64_t)(blockHeight / 720) < (uint64_t)(lastUpdateBlock / 720)) { // new epoch has already started
            feeToPay = prevFeeToPay;
            epochFees = prevEpochFees;
            lastUpdateBlock = blockHeight;
        }
        if (blockHeight % 720 == 0) {
            prevFeeToPay = feeToPay;
            feeToPay = static_cast<unsigned int>((double)epochFees / 720);
            epochFees = 0;
        }
        writeToFile(); // update data in the file
        return feeToPay;
    }

    void readFromFile() {
		std::string text;
		uint64_t blockHeight, lowPrice, highPrice;

		std::ifstream fd("priceData.txt");
		if (fd && !fd.eof()) {
			try {
				if (!fd.eof()) {
					getline(fd, text);
					currentMultiplier = std::stod(text);
				} else {
					CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: currentMultiplier not found\n";
				}
				if (!fd.eof()) {
					getline(fd, text);
					lastUpdateBlock = std::stoul(text);
				} else {
					CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: lastUpdateBlock not found\n";
				}
				if (!fd.eof()) {
					getline(fd, text);
					epochFees = std::stoul(text);
				} else {
					CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: epochFees not found\n";
				}
				if (!fd.eof()) {
					getline(fd, text);
					feeToPay = std::stoul(text);
				} else {
					CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: feeToPay not found\n";
				}
				if (!fd.eof()) {
					getline(fd, text);
					totalSupply = std::stoul(text);
				} else {
					CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: totalSupply not found\n";
				}
				while (!fd.eof()) {
					getline(fd, text);
					blockHeight = std::stoul(text);
					if (!fd.eof()) {
						getline(fd, text);
						lowPrice = std::stoul(text);
					}  else {
						CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: lowPrice not found\n";
					}
					if (!fd.eof()) {
						getline(fd, text);
						highPrice = std::stoul(text);
					} else {
						CATAPULT_LOG(error) << "Error: data in priceData.txt is incomplete: highPrice not found\n";
					}
					priceList.push_front({blockHeight, lowPrice, highPrice});
				}
			} catch(const std::exception & ex) {
				CATAPULT_LOG(error) << "Error: Problem with reading data from priceData.txt\n";
                CATAPULT_LOG(error) << ex.what() << "\n";
			}
		}
    }

    void writeToFile() {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
		std::ofstream fr("priceData.txt");
        fr << currentMultiplier << "\n";
        fr << lastUpdateBlock << "\n";
        fr << epochFees << "\n";
        fr << feeToPay << "\n";
        fr << totalSupply; // avoid adding new lines in the end of a file
        
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            fr << "\n";
            fr << std::get<0>(*it) << "\n";
            fr << std::get<1>(*it) << "\n";
            fr << std::get<2>(*it);
        }
    }
}}
        