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

#include "FinalizationMessageTestUtils.h"
#include "catapult/cache_core/AccountStateCache.h"
#include "catapult/crypto_voting/OtsTree.h"
#include "catapult/model/BlockUtils.h"
#include "tests/test/cache/AccountStateCacheTestUtils.h"
#include "tests/test/core/mocks/MockMemoryStream.h"
#include "tests/test/nodeps/KeyTestUtils.h"
#include "tests/test/nodeps/Nemesis.h"
#include "tests/test/nodeps/Random.h"
#include "tests/TestHarness.h"

namespace catapult { namespace test {

	// region message factories

	std::unique_ptr<model::FinalizationMessage> CreateMessage(const Hash256& hash) {
		return CreateMessage(GenerateRandomValue<Height>(), hash);
	}

	std::unique_ptr<model::FinalizationMessage> CreateMessage(Height height, const Hash256& hash) {
		auto pMessage = CreateMessage({ Random(), Random(), Random() }, hash);
		pMessage->Height = height;
		return pMessage;
	}

	std::unique_ptr<model::FinalizationMessage> CreateMessage(Height height, uint32_t numHashes) {
		uint32_t messageSize = SizeOf32<model::FinalizationMessage>() + numHashes * static_cast<uint32_t>(Hash256::Size);
		auto pMessage = utils::MakeUniqueWithSize<model::FinalizationMessage>(messageSize);
		test::FillWithRandomData({ reinterpret_cast<uint8_t*>(pMessage.get()), messageSize });
		pMessage->Size = messageSize;
		pMessage->HashesCount = numHashes;
		pMessage->Height = height;
		return pMessage;
	}

	std::unique_ptr<model::FinalizationMessage> CreateMessage(const crypto::StepIdentifier& stepIdentifier, const Hash256& hash) {
		uint32_t messageSize = SizeOf32<model::FinalizationMessage>() + static_cast<uint32_t>(Hash256::Size);
		auto pMessage = utils::MakeUniqueWithSize<model::FinalizationMessage>(messageSize);
		pMessage->Size = messageSize;
		pMessage->HashesCount = 1;
		pMessage->StepIdentifier = stepIdentifier;

		FillWithRandomData(pMessage->Signature);
		*pMessage->HashesPtr() = hash;
		return pMessage;
	}

	std::unique_ptr<model::FinalizationMessage> CreateValidNemesisMessage(
			const crypto::StepIdentifier& stepIdentifier,
			const Hash256& hash,
			const AccountKeyPairDescriptor& keyPairDescriptor) {
		return CreateValidMessage(stepIdentifier, Height(2), hash, keyPairDescriptor);
	}

	std::unique_ptr<model::FinalizationMessage> CreateValidMessage(
			const crypto::StepIdentifier& stepIdentifier,
			Height height,
			const Hash256& hash,
			const AccountKeyPairDescriptor& keyPairDescriptor) {
		auto pMessage = CreateMessage(stepIdentifier, hash);
		pMessage->Height = height;

		SignMessage(*pMessage, keyPairDescriptor.VotingKeyPair);
		return pMessage;
	}

	// endregion

	// region message utils

	void SignMessage(model::FinalizationMessage& message, const crypto::KeyPair& votingKeyPair) {
		auto storage = mocks::MockSeekableMemoryStream();
		auto otsTree = crypto::OtsTree::Create(
				CopyKeyPair(votingKeyPair),
				storage,
				FinalizationPoint(1),
				FinalizationPoint(20),
				{ 20, 20 });
		message.Signature = otsTree.sign(message.StepIdentifier, {
			reinterpret_cast<const uint8_t*>(&message) + model::FinalizationMessage::Header_Size,
			message.Size - model::FinalizationMessage::Header_Size
		});
	}

	void AssertEqualMessage(
			const model::FinalizationMessage& expected,
			const model::FinalizationMessage& actual,
			const std::string& message) {
		ASSERT_EQ(expected.Size, actual.Size) << message;
		EXPECT_EQ_MEMORY(&expected, &actual, expected.Size) << message;
	}

	// endregion

	// region account state cache utils

	std::vector<AccountKeyPairDescriptor> AddAccountsWithBalances(
			cache::AccountStateCacheDelta& accountStateCacheDelta,
			Height height,
			MosaicId mosaicId,
			const std::vector<Amount>& balances) {
		std::vector<AccountKeyPairDescriptor> keyPairDescriptors;
		for (auto balance : balances) {
			keyPairDescriptors.emplace_back(GenerateKeyPair());

			auto address = GenerateRandomByteArray<Address>();
			accountStateCacheDelta.addAccount(address, height);
			auto& accountState = accountStateCacheDelta.find(address).get();
			accountState.SupplementalPublicKeys.voting().add({
				keyPairDescriptors.back().VotingPublicKey,
				FinalizationPoint(1),
				FinalizationPoint(100)
			});
			accountState.Balances.credit(mosaicId, balance);
		}

		accountStateCacheDelta.updateHighValueAccounts(height);
		return keyPairDescriptors;
	}

	// endregion
}}