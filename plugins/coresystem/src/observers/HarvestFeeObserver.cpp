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
			
			uint64_t feeToPay = notification.feeToPay;
			uint64_t inflation = notification.inflation;
			Amount inflationAmount = Amount(inflation);
			Amount totalAmount = Amount(inflation + feeToPay);

			if (catapult::plugins::generationCeiling == 0) {
				catapult::plugins::readConfig();
			}
			catapult::plugins::priceList.clear();
			catapult::plugins::loadPricesFromFile(context.Height.unwrap());
			double average30, average60, average90, average120;
			catapult::plugins::getAverage(context.Height.unwrap(), average30, average60, average90, average120);
			double increase30 = average30 / average60;
			double increase60 = catapult::plugins::areSame(average90, 0) ? 0 : average60 / average90;
			double increase90 = catapult::plugins::areSame(average120, 0) ? 0 : average90 / average120;
			double multiplier = catapult::plugins::getMultiplier(increase30, increase60, increase90);

			if (context.Height.unwrap() == 1) {
				totalAmount = Amount(0);
				inflationAmount = Amount(0);
			}
			auto networkAmount = Amount(totalAmount.unwrap() * options.HarvestNetworkPercentage / 100);
			auto beneficiaryAmount = ShouldShareFees(notification, options.HarvestBeneficiaryPercentage)
					? Amount(totalAmount.unwrap() * options.HarvestBeneficiaryPercentage / 100)
					: Amount();
			auto harvesterAmount = totalAmount - networkAmount - beneficiaryAmount;

			CATAPULT_LOG(error) << "BLOCK INFORMATION";
			CATAPULT_LOG(error) << "Block: " << context.Height.unwrap();
			CATAPULT_LOG(error) << "Commit: " << (NotifyMode::Commit == context.Mode);
			CATAPULT_LOG(error) << "Beneficiary: " << model::AddressToString(notification.Beneficiary);
			CATAPULT_LOG(error) << "Amount: " << beneficiaryAmount.unwrap();
			CATAPULT_LOG(error) << "Harvester: " << model::AddressToString(notification.Harvester);
			CATAPULT_LOG(error) << "Amount: " << harvesterAmount.unwrap();
			CATAPULT_LOG(error) << "Total block fees: " << notification.TotalFee;
			CATAPULT_LOG(error) << "Fee To Pay: " << feeToPay;
			CATAPULT_LOG(error) << "Inflation: " << inflation;
			CATAPULT_LOG(error) << "Total Supply: " << notification.totalSupply;
			CATAPULT_LOG(error) << "Collected Fees: " << notification.collectedEpochFees;
			CATAPULT_LOG(error) << "Inflation multiplier: " << notification.inflationMultiplier;
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
				model::InflationReceipt average30Receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount((int64_t)(average30 * 100000)));
				model::InflationReceipt average60Receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount((int64_t)(average60 * 100000)));
				model::InflationReceipt average90Receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount((int64_t)(average90 * 100000)));
				model::InflationReceipt average120Receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount((int64_t)(average120 * 100000)));
				model::InflationReceipt multiplierReceipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, Amount((int64_t)(multiplier * 100000)));
				context.StatementBuilder().addReceipt(receipt);
				context.StatementBuilder().addReceipt(average30Receipt);
				context.StatementBuilder().addReceipt(average60Receipt);
				context.StatementBuilder().addReceipt(average90Receipt);
				context.StatementBuilder().addReceipt(average120Receipt);
				context.StatementBuilder().addReceipt(multiplierReceipt);
			}
		}));
	}
}}
