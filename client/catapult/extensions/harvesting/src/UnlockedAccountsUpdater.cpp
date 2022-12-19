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

#include "UnlockedAccountsUpdater.h"
#include "UnlockedAccounts.h"
#include "UnlockedFileQueueConsumer.h"
#include "catapult/cache/CatapultCache.h"
#include "catapult/cache_core/ImportanceView.h"
#include "catapult/io/RawFile.h"

namespace catapult { namespace harvesting {

	namespace {
		size_t PruneUnlockedAccounts(UnlockedAccounts& unlockedAccounts, const cache::CatapultCache& cache) {
			auto cacheView = cache.createView();
			auto height = cacheView.height() + Height(1);
			auto readOnlyAccountStateCache = cache::ReadOnlyAccountStateCache(cacheView.sub<cache::AccountStateCache>());
			size_t numPrunedAccounts = 0;
			unlockedAccounts.modifier().removeIf([height, &readOnlyAccountStateCache, &numPrunedAccounts](const auto& key) {
				cache::ImportanceView view(readOnlyAccountStateCache);
				auto shouldPruneAccount = !view.canHarvest(key, height);
				if (shouldPruneAccount)
					++numPrunedAccounts;

				return shouldPruneAccount;
			});

			return numPrunedAccounts;
		}

		bool AddToUnlocked(UnlockedAccounts& unlockedAccounts, BlockGeneratorKeyPairs&& keyPairs) {
			auto addResult = unlockedAccounts.modifier().add(std::move(keyPairs));
			if (UnlockedAccountsAddResult::Success_New == addResult) {
				CATAPULT_LOG(info) << "added NEW account " << keyPairs.signingKeyPair().publicKey();
				return true;
			}

			return false;
		}

		bool RemoveFromUnlocked(UnlockedAccounts& unlockedAccounts, const Key& publicKey) {
			if (unlockedAccounts.modifier().remove(publicKey)) {
				CATAPULT_LOG(info) << "removed account " << publicKey;
				return true;
			}

			return false;
		}
	}

	UnlockedAccountsUpdater::UnlockedAccountsUpdater(
			const cache::CatapultCache& cache,
			UnlockedAccounts& unlockedAccounts,
			const crypto::KeyPair& encryptionKeyPair,
			const config::CatapultDataDirectory& dataDirectory)
			: m_cache(cache)
			, m_unlockedAccounts(unlockedAccounts)
			, m_encryptionKeyPair(encryptionKeyPair)
			, m_dataDirectory(dataDirectory)
			, m_harvestersFilename(m_dataDirectory.rootDir().file("harvesters.dat"))
			, m_unlockedAccountsStorage(m_harvestersFilename)
	{}

	void UnlockedAccountsUpdater::load() {
		// load entries
		m_unlockedAccountsStorage.load(m_encryptionKeyPair, [&unlockedAccounts = m_unlockedAccounts](auto&& keyPair) {
			// TODO: unlock account message will need to be updated to support multiple keys
			BlockGeneratorKeyPairs keyPairs(
					std::move(keyPair),
					crypto::KeyPair::FromString("0000000000000000000000000000000000000000000000000000000000000001"));
			AddToUnlocked(unlockedAccounts, std::move(keyPairs));
		});
	}

	void UnlockedAccountsUpdater::update() {
		// 1. process queued accounts
		bool hasAnyRemoval = false;
		auto processEntryKeyPair = [&unlockedAccounts = m_unlockedAccounts, &storage = m_unlockedAccountsStorage, &hasAnyRemoval](
				const auto& unlockedEntryMessage,
				auto&& keyPair) {
			auto messageIdentifier = GetMessageIdentifier(unlockedEntryMessage);
			const auto& harvesterPublicKey = keyPair.publicKey();
			if (UnlockedEntryDirection::Add == unlockedEntryMessage.Direction) {
				// TODO: unlock account message will need to be updated to support multiple keys
				BlockGeneratorKeyPairs keyPairs(
						std::move(keyPair),
						crypto::KeyPair::FromString("0000000000000000000000000000000000000000000000000000000000000001"));
				if (!storage.contains(messageIdentifier) && AddToUnlocked(unlockedAccounts, std::move(keyPairs)))
					storage.add(messageIdentifier, unlockedEntryMessage.EncryptedEntry, harvesterPublicKey);
			} else {
				RemoveFromUnlocked(unlockedAccounts, harvesterPublicKey);
				storage.remove(messageIdentifier);
				hasAnyRemoval = true;
			}
		};
		UnlockedFileQueueConsumer(m_dataDirectory.dir("transfer_message"), m_encryptionKeyPair, processEntryKeyPair);

		// 2. prune accounts that are not eligible to harvest the next block
		auto numPrunedAccounts = PruneUnlockedAccounts(m_unlockedAccounts, m_cache);

		// 3. save accounts
		if (numPrunedAccounts > 0 || hasAnyRemoval) {
			auto view = m_unlockedAccounts.view();
			m_unlockedAccountsStorage.save([&view](const auto& harvesterPublicKey) {
				return view.contains(harvesterPublicKey);
			});
		}
	}
}}
