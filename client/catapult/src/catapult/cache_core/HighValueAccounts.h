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
#include "AccountStateCacheTypes.h"
#include "catapult/model/ContainerTypes.h"

namespace catapult { namespace cache {

	/// High value accounts container.
	class HighValueAccounts {
	public:
		/// Creates an empty container.
		HighValueAccounts();

		/// Creates a container around \a addresses.
		explicit HighValueAccounts(const model::AddressSet& addresses);

		/// Creates a container around \a addresses.
		explicit HighValueAccounts(model::AddressSet&& addresses);

	public:
		/// Gets the high value addresses.
		const model::AddressSet& addresses() const;

	private:
		model::AddressSet m_addresses;
	};

	/// High value accounts updater.
	class HighValueAccountsUpdater {
	private:
		using MemorySetType = AccountStateCacheTypes::PrimaryTypes::BaseSetDeltaType::SetType::MemorySetType;

	public:
		/// Creates an updater around \a options and existing \a addresses.
		HighValueAccountsUpdater(AccountStateCacheTypes::Options options, const model::AddressSet& addresses);

	public:
		/// Gets the height of the update operation.
		Height height() const;

		/// Gets the (current) high value addresses.
		const model::AddressSet& addresses() const;

		/// Gets the (removed) high value addresses relative to the initial addresses.
		const model::AddressSet& removedAddresses() const;

	public:
		/// Sets the \a height of the update operation.
		void setHeight(Height height);

		/// Updates high value accounts based on changes described in \a deltas.
		void update(const deltaset::DeltaElements<MemorySetType>& deltas);

	public:
		/// Detaches the underlying data associated with this updater and converts it to a high value accounts container.
		HighValueAccounts detachAccounts();

	private:
		AccountStateCacheTypes::Options m_options;
		const model::AddressSet& m_original;
		model::AddressSet m_current;
		model::AddressSet m_removed;
		Height m_height;
	};
}}
