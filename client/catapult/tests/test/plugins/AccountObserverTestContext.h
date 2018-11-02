/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
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

#pragma once
#include "ObserverTestContext.h"
#include "catapult/cache_core/AccountStateCache.h"

namespace catapult { namespace test {

	/// Observer test context that wraps an observer context and exposes functions for interacting with the account state cache.
	class AccountObserverTestContext : public test::ObserverTestContext {
	public:
		using ObserverTestContext::ObserverTestContext;

	public:
		/// Finds the account identified by \a address.
		const state::AccountState* find(const Address& address) const {
			return cache().sub<cache::AccountStateCache>().find(address).tryGet();
		}

		/// Finds the account identified by \a publicKey.
		const state::AccountState* find(const Key& publicKey) const {
			return cache().sub<cache::AccountStateCache>().find(publicKey).tryGet();
		}

	private:
		state::AccountState& addAccount(const Address& address) {
			auto& accountStateCache = cache().sub<cache::AccountStateCache>();
			accountStateCache.addAccount(address, Height(1234));
			return accountStateCache.find(address).get();
		}

		state::AccountState& addAccount(const Key& publicKey) {
			auto& accountStateCache = cache().sub<cache::AccountStateCache>();
			accountStateCache.addAccount(publicKey, Height(1));
			return accountStateCache.find(publicKey).get();
		}

	public:
		/// Sets the (xem) balance of the account identified by \a accountIdentifier to \a amount.
		template<typename IdType>
		state::AccountBalances& setAccountBalance(const IdType& accountIdentifier, Amount::ValueType amount) {
			return setAccountBalance(accountIdentifier, Amount(amount));
		}

		/// Sets the (xem) balance of the account identified by \a accountIdentifier to \a amount.
		template<typename IdType>
		state::AccountBalances& setAccountBalance(const IdType& accountIdentifier, Amount amount) {
			auto& accountState = addAccount(accountIdentifier);
			accountState.Balances.credit(Xem_Id, amount);
			return accountState.Balances;
		}

		/// Gets the (xem) balance of the account identified by \a accountIdentifier.
		template<typename IdType>
		Amount getAccountBalance(const IdType& accountIdentifier) const {
			auto pAccountState = find(accountIdentifier);
			if (!pAccountState)
				CATAPULT_THROW_RUNTIME_ERROR_1("could not find account in cache", utils::HexFormat(accountIdentifier));

			return pAccountState->Balances.get(Xem_Id);
		}
	};
}}
