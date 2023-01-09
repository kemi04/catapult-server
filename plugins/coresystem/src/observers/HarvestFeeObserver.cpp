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

#include "Observers.h"
#include "catapult/cache_core/AccountStateCache.h"
#include "catapult/cache_core/AccountStateCacheUtils.h"
#include "catapult/model/InflationCalculator.h"
#include "catapult/model/Mosaic.h"
#include "catapult/utils/Logging.h"
#include "catapult/model/Address.h"
#include "catapult/model/priceUtil.cpp"
// Not ideal but the implementation file can't be found otherwise before the header is included

namespace catapult { namespace observers {

	namespace {
		using Notification = model::BlockNotification;

		class FeeApplier {
		public:
			FeeApplier(MosaicId currencyMosaicId, ObserverContext& context)
					: m_currencyMosaicId(currencyMosaicId)
					, m_context(context)
			{}

		public:
			void apply(const Address& address, Amount amount) {
				auto& cache = m_context.Cache.sub<cache::AccountStateCache>();
				auto feeMosaic = model::Mosaic{ m_currencyMosaicId, amount };
				cache::ProcessForwardedAccountState(cache, address, [&feeMosaic, &context = m_context](auto& accountState) {
					ApplyFee(accountState, context.Mode, feeMosaic, context.StatementBuilder());
				});
			}

		private:
			static void ApplyFee(
					state::AccountState& accountState,
					NotifyMode notifyMode,
					const model::Mosaic& feeMosaic,
					ObserverStatementBuilder& statementBuilder) {
				if (NotifyMode::Rollback == notifyMode) {
					accountState.Balances.debit(feeMosaic.MosaicId, feeMosaic.Amount);
					return;
				}

				accountState.Balances.credit(feeMosaic.MosaicId, feeMosaic.Amount);

				// add fee receipt
				auto receiptType = model::Receipt_Type_Harvest_Fee;
				model::BalanceChangeReceipt receipt(receiptType, accountState.Address, feeMosaic.MosaicId, feeMosaic.Amount);
				statementBuilder.addReceipt(receipt);
			}

		private:
			MosaicId m_currencyMosaicId;
			ObserverContext& m_context;
		};

		bool ShouldShareFees(const Notification& notification, uint8_t harvestBeneficiaryPercentage) {
			return 0u < harvestBeneficiaryPercentage && notification.Harvester != notification.Beneficiary;
		}
	}

	DECLARE_OBSERVER(HarvestFee, Notification)(const HarvestFeeOptions& options, const model::InflationCalculator& calculator) {
		return MAKE_OBSERVER(HarvestFee, Notification, ([options, calculator](const Notification& notification, ObserverContext& context) {
			uint64_t inflation;
			uint64_t totalSupply;
			uint64_t feeToPay;
			uint64_t collectedFees;
			double inflationMultiplier;
			catapult::plugins::ActiveValues* activeValues = catapult::plugins::priceDrivenModel->isSync ?
				&catapult::plugins::priceDrivenModel->syncActiveValues :
				nullptr;

			// If the block comes from the harvester extension or is in rollback mode, don't validate it
			if (activeValues == nullptr || NotifyMode::Rollback == context.Mode) {
				inflation = notification.inflation;
				totalSupply = notification.totalSupply;
				feeToPay = notification.feeToPay;
				collectedFees = notification.collectedEpochFees;
				inflationMultiplier = notification.inflationMultiplier;
			} else {
				if (context.Height.unwrap() % catapult::plugins::priceDrivenModel->config.feeRecalculationFrequency == 0) {
					activeValues->feeToPay = 
						static_cast<unsigned int>(static_cast<double>(activeValues->collectedFees) / 
						static_cast<double>(catapult::plugins::priceDrivenModel->config.feeRecalculationFrequency) + 0.5);

					activeValues->collectedFees = notification.TotalFee.unwrap();
				} else {
					activeValues->collectedFees += notification.TotalFee.unwrap();
				}
				
				if (context.Height.unwrap() % catapult::plugins::priceDrivenModel->config.multiplierRecalculationFrequency == 0) {
					double increase = catapult::plugins::priceDrivenModel->getCoinGenerationMultiplier(context.Height.unwrap());
					activeValues->inflationMultiplier += increase;
					if (catapult::plugins::priceDrivenModel->areSame(increase, 0)) {
						activeValues->inflationMultiplier = 0;
					} else if (activeValues->inflationMultiplier > 94) {
						activeValues->inflationMultiplier = 94;
					}
				}

				inflation = static_cast<uint64_t>(static_cast<double>(activeValues->totalSupply) / 105120000 * (2 + activeValues->inflationMultiplier) + 0.5);
				activeValues->totalSupply += inflation;
				totalSupply = activeValues->totalSupply;
				feeToPay = activeValues->feeToPay;
				collectedFees = activeValues->collectedFees;
				inflationMultiplier = activeValues->inflationMultiplier;
			}

			Amount inflationAmount = Amount(inflation);
			Amount totalAmount = Amount(inflation + feeToPay);

			auto networkAmount = Amount(totalAmount.unwrap() * options.HarvestNetworkPercentage / 100);
			auto beneficiaryAmount = ShouldShareFees(notification, options.HarvestBeneficiaryPercentage)
					? Amount(totalAmount.unwrap() * options.HarvestBeneficiaryPercentage / 100)
					: Amount();
			auto harvesterAmount = totalAmount - networkAmount - beneficiaryAmount;

			CATAPULT_LOG(error) << "BLOCK INFORMATION";
			CATAPULT_LOG(error) << "Block: " << context.Height.unwrap();
			CATAPULT_LOG(error) << "Commit: " << (NotifyMode::Commit == context.Mode);
			CATAPULT_LOG(error) << "Synchronizer extension: " << catapult::plugins::priceDrivenModel->isSync;
			CATAPULT_LOG(error) << "Beneficiary: " << model::AddressToString(notification.Beneficiary);
			CATAPULT_LOG(error) << "Amount: " << beneficiaryAmount.unwrap();
			CATAPULT_LOG(error) << "Harvester: " << model::AddressToString(notification.Harvester);
			CATAPULT_LOG(error) << "Amount: " << harvesterAmount.unwrap();
			CATAPULT_LOG(error) << "Total block fees: " << notification.TotalFee;
			CATAPULT_LOG(error) << "Fee To Pay: " << feeToPay << " vs " << notification.feeToPay;
			CATAPULT_LOG(error) << "Inflation: " << inflation << " vs " << notification.inflation;
			CATAPULT_LOG(error) << "Total Supply: " << totalSupply << " vs " << notification.totalSupply;
			CATAPULT_LOG(error) << "Collected Fees: " << collectedFees << " vs " << notification.collectedEpochFees;
			CATAPULT_LOG(error) << "Inflation multiplier: " << inflationMultiplier << " vs " << notification.inflationMultiplier;
			CATAPULT_LOG(error) << "";

			// always create receipt for harvester
			FeeApplier applier(options.CurrencyMosaicId, context);
			applier.apply(notification.Harvester, harvesterAmount);

			// only if amount is non-zero create receipt for network sink account
			if (Amount() != networkAmount)
				applier.apply(options.HarvestNetworkFeeSinkAddress.get(context.Height), networkAmount);

			// only if amount is non-zero create receipt for beneficiary account
			if (Amount() != beneficiaryAmount)
				applier.apply(notification.Beneficiary, beneficiaryAmount);

			// add inflation receipt
			if (Amount() != inflationAmount && NotifyMode::Commit == context.Mode) {
				model::InflationReceipt receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, inflationAmount);
				model::InflationReceipt inflationMultiplierReceipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, 
					Amount(static_cast<uint64_t>(inflationMultiplier)));
				model::InflationReceipt totalSupplyReceipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount(totalSupply));
				model::InflationReceipt feeToPayReceipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount(feeToPay));
				model::InflationReceipt collectedFeesReceipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount(collectedFees));

				context.StatementBuilder().addReceipt(receipt);
				context.StatementBuilder().addReceipt(inflationMultiplierReceipt);
				context.StatementBuilder().addReceipt(totalSupplyReceipt);
				context.StatementBuilder().addReceipt(feeToPayReceipt);
				context.StatementBuilder().addReceipt(collectedFeesReceipt);
			}
		}));
	}
}}
