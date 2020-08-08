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

#include "finalization/src/chain/RoundMessageAggregator.h"
#include "finalization/src/chain/RoundContext.h"
#include "finalization/src/model/FinalizationContext.h"
#include "catapult/cache_core/AccountStateCache.h"
#include "catapult/utils/MemoryUtils.h"
#include "finalization/tests/test/FinalizationMessageTestUtils.h"
#include "tests/test/cache/AccountStateCacheTestUtils.h"
#include "tests/test/nodeps/LockTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace chain {

#define TEST_CLASS RoundMessageAggregatorTests

	namespace {
		constexpr auto Finalization_Point = FinalizationPoint(3);
		constexpr auto Last_Finalized_Height = Height(123);

		// region TestContext

		struct TestContextOptions {
			uint64_t MaxResponseSize = 10'000'000;
			uint32_t MaxHashesPerPoint = 100;
		};

		class TestContext {
		private:
			static constexpr auto Harvesting_Mosaic_Id = MosaicId(9876);

		public:
			TestContext(uint32_t size, uint32_t threshold) : TestContext(size, threshold, TestContextOptions())
			{}

			TestContext(uint32_t size, uint32_t threshold, const TestContextOptions& options) {
				auto config = finalization::FinalizationConfiguration::Uninitialized();
				config.Size = size;
				config.Threshold = threshold;
				config.MaxHashesPerPoint = options.MaxHashesPerPoint;

				// 15/20M voting eligible
				cache::AccountStateCache cache(cache::CacheConfiguration(), CreateOptions());
				m_keyPairDescriptors = AddAccountsWithBalances(cache, Last_Finalized_Height, {
					Amount(4'000'000), Amount(2'000'000), Amount(1'000'000), Amount(2'000'000), Amount(3'000'000), Amount(4'000'000),
					Amount(1'000'000), Amount(1'000'000), Amount(1'000'000), Amount(1'000'000)
				});

				m_pAggregator = std::make_unique<RoundMessageAggregator>(
						options.MaxResponseSize,
						CreateFinalizationContext(config, cache));
			}

		public:
			auto& aggregator() {
				return *m_pAggregator;
			}

		public:
			void signMessage(model::FinalizationMessage& message, size_t signerIndex) const {
				test::SignMessage(message, m_keyPairDescriptors[signerIndex].VotingKeyPair);
			}

		public:
			auto detach() {
				return std::move(m_pAggregator);
			}

		private:
			static cache::AccountStateCacheTypes::Options CreateOptions() {
				auto options = test::CreateDefaultAccountStateCacheOptions(MosaicId(1111), Harvesting_Mosaic_Id);
				options.MinVoterBalance = Amount(2'000'000);
				return options;
			}

			static std::vector<test::AccountKeyPairDescriptor> AddAccountsWithBalances(
					cache::AccountStateCache& cache,
					Height height,
					const std::vector<Amount>& balances) {
				auto delta = cache.createDelta();
				auto keyPairDescriptors = test::AddAccountsWithBalances(*delta, height, Harvesting_Mosaic_Id, balances);
				cache.commit();
				return keyPairDescriptors;
			}

			model::FinalizationContext CreateFinalizationContext(
					const finalization::FinalizationConfiguration& config,
					const cache::AccountStateCache& cache) {
				auto point = Finalization_Point;
				auto height = Last_Finalized_Height;
				auto generationHash = test::GenerateRandomByteArray<GenerationHash>();
				return model::FinalizationContext(point, height, generationHash, config, *cache.createView());
			}

		private:
			std::unique_ptr<RoundMessageAggregator> m_pAggregator;
			std::vector<test::AccountKeyPairDescriptor> m_keyPairDescriptors;
		};

		// endregion
	}

	// region constructor

	TEST(TEST_CLASS, CanCreateEmptyAggregator) {
		// Act:
		TestContext context(1000, 700);

		// Assert:
		auto view = context.aggregator().view();
		EXPECT_EQ(0u, view.size());

		EXPECT_EQ(Finalization_Point, view.finalizationContext().point());
		EXPECT_EQ(Last_Finalized_Height, view.finalizationContext().height());
		EXPECT_EQ(Amount(15'000'000), view.finalizationContext().weight());

		EXPECT_EQ(0u, view.roundContext().size());
	}

	// endregion

	// region add - traits

	namespace {
		struct PrevoteTraits {
			static constexpr auto Round = 1;
			static constexpr auto Success_Result = RoundMessageAggregatorAddResult::Success_Prevote;
		};

		struct PrecommitTraits {
			static constexpr auto Round = 2;
			static constexpr auto Success_Result = RoundMessageAggregatorAddResult::Success_Precommit;
		};
	}

#define PREVOTE_PRECOMIT_TEST(TEST_NAME) \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)(); \
	TEST(TEST_CLASS, TEST_NAME##_Prevote) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<PrevoteTraits>(); } \
	TEST(TEST_CLASS, TEST_NAME##_Precommit) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<PrecommitTraits>(); } \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)()

	// endregion

	// region add - failure

	namespace {
		auto CreateValues(uint64_t base, std::initializer_list<int64_t> deltas) {
			std::vector<uint64_t> values;
			for (auto delta : deltas)
				values.push_back(static_cast<uint64_t>(static_cast<int64_t>(base) + delta));

			return values;
		}

		void AssertCannotAddMessage(
				RoundMessageAggregatorAddResult expectedResult,
				std::unique_ptr<model::FinalizationMessage>&& pMessage) {
			// Arrange:
			TestContext context(1000, 700);
			context.signMessage(*pMessage, 0);

			// Act:
			auto result = context.aggregator().modifier().add(std::move(pMessage));

			// Assert:
			EXPECT_EQ(expectedResult, result);
			EXPECT_EQ(0u, context.aggregator().view().size());
		}
	}

	PREVOTE_PRECOMIT_TEST(CannotAddMessageWithZeroHashes) {
		// Arrange:
		auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), 0);
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		// Act + Assert:
		AssertCannotAddMessage(RoundMessageAggregatorAddResult::Failure_Invalid_Hashes, std::move(pMessage));
	}

	PREVOTE_PRECOMIT_TEST(CannotAddMessageWithInvalidPoint) {
		// Arrange:
		for (auto point : CreateValues(Finalization_Point.unwrap(), { -2, -1, 1, 10 })) {
			auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), 1);
			pMessage->StepIdentifier = { point, TTraits::Round, 1 };

			// Act + Assert:
			AssertCannotAddMessage(RoundMessageAggregatorAddResult::Failure_Invalid_Point, std::move(pMessage));
		}
	}

	PREVOTE_PRECOMIT_TEST(CannotAddRedundantMessage) {
		// Arrange:
		auto pMessage = utils::UniqueToShared(test::CreateMessage(Last_Finalized_Height + Height(1), 1));
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage, 0);

		// Act:
		auto result1 = context.aggregator().modifier().add(pMessage);
		auto result2 = context.aggregator().modifier().add(pMessage);

		// Assert:
		EXPECT_EQ(TTraits::Success_Result, result1);
		EXPECT_EQ(RoundMessageAggregatorAddResult::Neutral_Redundant, result2);
		EXPECT_EQ(1u, context.aggregator().view().size());
	}

	PREVOTE_PRECOMIT_TEST(CannotAddMultipleMessagesFromSameSigner) {
		// Arrange:
		auto pMessage1 = test::CreateMessage(Last_Finalized_Height + Height(1), 1);
		pMessage1->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		auto pMessage2 = test::CreateMessage(Last_Finalized_Height + Height(1), 1);
		pMessage2->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage1, 0);
		context.signMessage(*pMessage2, 0);

		// Act:
		auto result1 = context.aggregator().modifier().add(std::move(pMessage1));
		auto result2 = context.aggregator().modifier().add(std::move(pMessage2));

		// Assert:
		EXPECT_EQ(TTraits::Success_Result, result1);
		EXPECT_EQ(RoundMessageAggregatorAddResult::Failure_Conflicting, result2);
		EXPECT_EQ(1u, context.aggregator().view().size());
	}

	PREVOTE_PRECOMIT_TEST(CannotAddMessageWithIneligibleSigner) {
		// Arrange:
		auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), 1);
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage, 2);

		// Act:
		auto result = context.aggregator().modifier().add(std::move(pMessage));

		// Assert:
		EXPECT_EQ(RoundMessageAggregatorAddResult::Failure_Processing, result);
		EXPECT_EQ(0u, context.aggregator().view().size());
	}

	PREVOTE_PRECOMIT_TEST(CannotAddMessageWithInvalidSignature) {
		// Arrange:
		auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), 1);
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage, 0);

		// - corrupt the signature
		pMessage->HashesPtr()[0][0] ^= 0xFF;

		// Act:
		auto result = context.aggregator().modifier().add(std::move(pMessage));

		// Assert:
		EXPECT_EQ(RoundMessageAggregatorAddResult::Failure_Processing, result);
		EXPECT_EQ(0u, context.aggregator().view().size());
	}

	namespace {
		template<typename TTraits>
		void AssertCannotAddMessageWithInvalidHeight(uint32_t numHashes, std::initializer_list<int64_t> heightDeltas) {
			// Arrange:
			for (auto height : CreateValues(Last_Finalized_Height.unwrap(), heightDeltas)) {
				auto pMessage = test::CreateMessage(Height(height), numHashes);
				pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

				// Act + Assert:
				AssertCannotAddMessage(RoundMessageAggregatorAddResult::Failure_Invalid_Height, std::move(pMessage));
			}
		}
	}

	TEST(TEST_CLASS, CannotAddMessageWithInvalidHeight_Prevote) {
		AssertCannotAddMessageWithInvalidHeight<PrevoteTraits>(10, { -122, -100, -50, -10 });
	}

	TEST(TEST_CLASS, CannotAddMessageWithInvalidHeight_Precommit) {
		AssertCannotAddMessageWithInvalidHeight<PrecommitTraits>(1, { -122, -100, -50, -10, -1 });
	}

	TEST(TEST_CLASS, CannotAddMessageWithMultipleHashes_Precommit) {
		// Arrange:
		auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), 2);
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), PrecommitTraits::Round, 1 };

		// Act + Assert:
		AssertCannotAddMessage(RoundMessageAggregatorAddResult::Failure_Invalid_Hashes, std::move(pMessage));
	}

	TEST(TEST_CLASS, CannotAddMessageWithGreaterThanMaxHashes_Prevote) {
		// Arrange:
		auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), TestContextOptions().MaxHashesPerPoint + 1);
		pMessage->StepIdentifier = { Finalization_Point.unwrap(), PrevoteTraits::Round, 1 };

		// Act + Assert:
		AssertCannotAddMessage(RoundMessageAggregatorAddResult::Failure_Invalid_Hashes, std::move(pMessage));
	}

	// endregion

	// region add -success

	namespace {
		template<typename TTraits>
		void AssertBasicAddSuccess(uint32_t numHashes, Height height) {
			// Arrange:
			auto pMessage = test::CreateMessage(height, numHashes);
			pMessage->StepIdentifier = { Finalization_Point.unwrap(), TTraits::Round, 1 };

			TestContext context(1000, 700);
			context.signMessage(*pMessage, 0);

			// Act:
			auto result = context.aggregator().modifier().add(std::move(pMessage));

			// Assert:
			EXPECT_EQ(TTraits::Success_Result, result);
			EXPECT_EQ(1u, context.aggregator().view().size());
		}
	}

	PREVOTE_PRECOMIT_TEST(CanAddMessageWithSingleHash) {
		AssertBasicAddSuccess<TTraits>(1, Last_Finalized_Height + Height(1));
	}

	PREVOTE_PRECOMIT_TEST(CanAddMessageWithSingleHashAtLastFinalizedHeight) {
		AssertBasicAddSuccess<TTraits>(1, Last_Finalized_Height);
	}

	TEST(TEST_CLASS, CanAddMessageWithMultipleHashes_Prevote) {
		AssertBasicAddSuccess<PrevoteTraits>(4, Last_Finalized_Height + Height(1));
	}

	TEST(TEST_CLASS, CanAddMessageWithMultipleHashesEndingAtLastFinalizedHeight_Prevote) {
		AssertBasicAddSuccess<PrevoteTraits>(4, Last_Finalized_Height - Height(3));
	}

	TEST(TEST_CLASS, CanAddMessageWithExactlyMaxHashes_Prevote) {
		AssertBasicAddSuccess<PrevoteTraits>(TestContextOptions().MaxHashesPerPoint, Last_Finalized_Height + Height(1));
	}

	TEST(TEST_CLASS, CanAddMessageWithLargerHeight_Precommit) {
		AssertBasicAddSuccess<PrecommitTraits>(1, Last_Finalized_Height + Height(7));
	}

	TEST(TEST_CLASS, CanAcceptPrevoteThenPrecommitMessageFromSameSigner) {
		// Arrange:
		auto pMessage1 = test::CreateMessage(Last_Finalized_Height + Height(1), 3);
		pMessage1->StepIdentifier = { Finalization_Point.unwrap(), PrevoteTraits::Round, 1 };

		auto pMessage2 = test::CreateMessage(Last_Finalized_Height + Height(2), 1);
		pMessage2->StepIdentifier = { Finalization_Point.unwrap(), PrecommitTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage1, 0);
		context.signMessage(*pMessage2, 0);

		// Act:
		auto result1 = context.aggregator().modifier().add(std::move(pMessage1));
		auto result2 = context.aggregator().modifier().add(std::move(pMessage2));

		// Assert:
		EXPECT_EQ(PrevoteTraits::Success_Result, result1);
		EXPECT_EQ(PrecommitTraits::Success_Result, result2);
		EXPECT_EQ(2u, context.aggregator().view().size());
	}

	TEST(TEST_CLASS, CanAcceptPrecommitThenPrevoteMessageFromSameSigner) {
		// Arrange:
		auto pMessage1 = test::CreateMessage(Last_Finalized_Height + Height(2), 1);
		pMessage1->StepIdentifier = { Finalization_Point.unwrap(), PrecommitTraits::Round, 1 };

		auto pMessage2 = test::CreateMessage(Last_Finalized_Height + Height(1), 3);
		pMessage2->StepIdentifier = { Finalization_Point.unwrap(), PrevoteTraits::Round, 1 };

		TestContext context(1000, 700);
		context.signMessage(*pMessage1, 0);
		context.signMessage(*pMessage2, 0);

		// Act:
		auto result1 = context.aggregator().modifier().add(std::move(pMessage1));
		auto result2 = context.aggregator().modifier().add(std::move(pMessage2));

		// Assert:
		EXPECT_EQ(PrecommitTraits::Success_Result, result1);
		EXPECT_EQ(PrevoteTraits::Success_Result, result2);
		EXPECT_EQ(2u, context.aggregator().view().size());
	}

	// endregion

	// region add - success (round context delegation)

	namespace {
		auto CreatePrevoteMessages(size_t numMessages, const Hash256* pHashes, size_t numHashes) {
			std::vector<std::shared_ptr<model::FinalizationMessage>> messages;
			for (auto i = 0u; i < numMessages; ++i) {
				auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1), static_cast<uint32_t>(numHashes));
				pMessage->StepIdentifier = { Finalization_Point.unwrap(), PrevoteTraits::Round, 1 };
				std::copy(pHashes, pHashes + numHashes, pMessage->HashesPtr());
				messages.push_back(std::move(pMessage));
			}

			return messages;
		}

		auto CreatePrecommitMessages(size_t numMessages, const Hash256* pHashes, size_t index) {
			std::vector<std::shared_ptr<model::FinalizationMessage>> messages;
			for (auto i = 0u; i < numMessages; ++i) {
				auto pMessage = test::CreateMessage(Last_Finalized_Height + Height(1 + index), 1);
				pMessage->StepIdentifier = { Finalization_Point.unwrap(), PrecommitTraits::Round, 1 };
				*pMessage->HashesPtr() = pHashes[index];
				messages.push_back(std::move(pMessage));
			}

			return messages;
		}

		void SignAllMessages(
				const TestContext& context,
				const std::vector<size_t>& signerIndexes,
				std::vector<std::shared_ptr<model::FinalizationMessage>>& messages) {
			for (auto i = 0u; i < messages.size(); ++i)
				context.signMessage(*messages[i], signerIndexes[i]);
		}
	}

	TEST(TEST_CLASS, CanDiscoverBestPrevoteFromAcceptedMessages) {
		// Arrange: only setup a prevote on the first 6/7 hashes
		auto prevoteHashes = test::GenerateRandomDataVector<Hash256>(7);
		auto prevoteMessages = CreatePrevoteMessages(4, prevoteHashes.data(), prevoteHashes.size());
		prevoteMessages.back()->Size -= static_cast<uint32_t>(Hash256::Size);
		--prevoteMessages.back()->HashesCount;

		// - sign with weights { 4M, 2M, 3M, 4M } (13M) > 15M * 0.7 (10.5M)
		TestContext context(1000, 700);
		SignAllMessages(context, { 5, 1, 4, 0 }, prevoteMessages);

		// - add all but one prevote message
		for (auto i = 0u; i < prevoteMessages.size() - 1; ++i)
			context.aggregator().modifier().add(prevoteMessages[i]);

		// Sanity:
		EXPECT_FALSE(context.aggregator().view().roundContext().tryFindBestPrevote().second);

		// Act:
		auto result = context.aggregator().modifier().add(prevoteMessages.back());

		// Assert:
		EXPECT_EQ(PrevoteTraits::Success_Result, result);

		auto view = context.aggregator().view();
		EXPECT_EQ(4u, view.size());

		const auto& bestPrevoteResultPair = view.roundContext().tryFindBestPrevote();
		EXPECT_TRUE(bestPrevoteResultPair.second);
		EXPECT_EQ(model::HeightHashPair({ Last_Finalized_Height + Height(6), prevoteHashes[5] }), bestPrevoteResultPair.first);

		EXPECT_FALSE(view.roundContext().tryFindBestPrecommit().second);

		EXPECT_FALSE(view.roundContext().isCompletable());
	}

	TEST(TEST_CLASS, CanDiscoverBestPrecommitFromAcceptedMessages) {
		// Arrange: only setup a prevote on the first 6/7 hashes
		auto prevoteHashes = test::GenerateRandomDataVector<Hash256>(7);
		auto prevoteMessages = CreatePrevoteMessages(4, prevoteHashes.data(), prevoteHashes.size());
		prevoteMessages.back()->Size -= static_cast<uint32_t>(Hash256::Size);
		--prevoteMessages.back()->HashesCount;

		// - only setup a precommit on the first 3/7 hashes
		auto precommitMessages = CreatePrecommitMessages(4, prevoteHashes.data(), 3);
		precommitMessages.back()->Height = precommitMessages.back()->Height - Height(1);
		*precommitMessages.back()->HashesPtr() = prevoteHashes[2];

		// - sign prevotes with weights { 4M, 2M, 3M, 4M } (13M) > 15M * 0.7 (10.5M)
		// - sign precommits with weights { 2M, 2M, 4M, 3M } (11M) > 15M * 0.7 (10.5M)
		TestContext context(1000, 700);
		SignAllMessages(context, { 5, 1, 4, 0 }, prevoteMessages);
		SignAllMessages(context, { 3, 1, 0, 4 }, precommitMessages);

		// - add all prevote messages
		for (const auto& pMessage : prevoteMessages)
			context.aggregator().modifier().add(pMessage);

		// - add all but one precommit message
		for (auto i = 0u; i < precommitMessages.size() - 1; ++i)
			context.aggregator().modifier().add(precommitMessages[i]);

		// Sanity:
		EXPECT_TRUE(context.aggregator().view().roundContext().tryFindBestPrevote().second);
		EXPECT_FALSE(context.aggregator().view().roundContext().tryFindBestPrecommit().second);

		// Act:
		auto result = context.aggregator().modifier().add(precommitMessages.back());

		// Assert:
		EXPECT_EQ(PrecommitTraits::Success_Result, result);

		auto view = context.aggregator().view();
		EXPECT_EQ(8u, view.size());

		const auto& bestPrevoteResultPair = view.roundContext().tryFindBestPrevote();
		EXPECT_TRUE(bestPrevoteResultPair.second);
		EXPECT_EQ(model::HeightHashPair({ Last_Finalized_Height + Height(6), prevoteHashes[5] }), bestPrevoteResultPair.first);

		const auto& bestPrecommitResultPair = view.roundContext().tryFindBestPrecommit();
		EXPECT_TRUE(bestPrecommitResultPair.second);
		EXPECT_EQ(model::HeightHashPair({ Last_Finalized_Height + Height(3), prevoteHashes[2] }), bestPrecommitResultPair.first);

		EXPECT_TRUE(view.roundContext().isCompletable());
	}

	// endregion

	// region shortHashes

	namespace {
		template<typename TAction>
		void RunSeededAggregatorTest(TAction action) {
			// Arrange: add 7 messages (4 prevotes and 3 precommits)
			auto prevoteHashes = test::GenerateRandomDataVector<Hash256>(7);
			auto prevoteMessages = CreatePrevoteMessages(4, prevoteHashes.data(), prevoteHashes.size());
			prevoteMessages.back()->Size -= static_cast<uint32_t>(Hash256::Size);
			--prevoteMessages.back()->HashesCount;

			auto precommitMessages = CreatePrecommitMessages(3, prevoteHashes.data(), 3);
			precommitMessages.back()->Height = precommitMessages.back()->Height - Height(1);
			*precommitMessages.back()->HashesPtr() = prevoteHashes[2];

			// - sign the messages
			TestContext context(1000, 900);
			SignAllMessages(context, { 5, 1, 4, 0 }, prevoteMessages);
			SignAllMessages(context, { 3, 1, 0 }, precommitMessages);

			// - add the messages
			std::vector<utils::ShortHash> shortHashes;
			for (const auto& pMessage : prevoteMessages) {
				context.aggregator().modifier().add(pMessage);
				shortHashes.push_back(utils::ToShortHash(model::CalculateMessageHash(*pMessage)));
			}

			for (const auto& pMessage : precommitMessages) {
				context.aggregator().modifier().add(pMessage);
				shortHashes.push_back(utils::ToShortHash(model::CalculateMessageHash(*pMessage)));
			}

			// Sanity:
			EXPECT_EQ(7u, shortHashes.size());

			// Act + Assert:
			action(context.aggregator(), shortHashes);
		}
	}

	TEST(TEST_CLASS, ShortHashesReturnsNoShortHashesWhenAggregtorIsEmpty) {
		// Arrange:
		TestContext context(1000, 700);

		// Act:
		auto shortHashes = context.aggregator().view().shortHashes();

		// Assert:
		EXPECT_TRUE(shortHashes.empty());
	}

	TEST(TEST_CLASS, ShortHashesReturnsShortHashesForAllMessages) {
		// Arrange:
		RunSeededAggregatorTest([](const auto& aggregator, const auto& seededShortHashes) {
			// Act:
			auto shortHashes = aggregator.view().shortHashes();

			// Assert:
			EXPECT_EQ(7u, shortHashes.size());
			EXPECT_EQ(
					utils::ShortHashesSet(seededShortHashes.cbegin(), seededShortHashes.cend()),
					utils::ShortHashesSet(shortHashes.cbegin(), shortHashes.cend()));
		});
	}

	// endregion

	// region unknownMessages

	namespace {
		auto ToShortHashes(const std::vector<std::shared_ptr<const model::FinalizationMessage>>& messages) {
			utils::ShortHashesSet shortHashes;
			for (const auto& pMessage : messages)
				shortHashes.insert(utils::ToShortHash(model::CalculateMessageHash(*pMessage)));

			return shortHashes;
		}
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsNoMessagesWhenAggregatorIsEmpty) {
		// Arrange:
		TestContext context(1000, 700);

		// Act:
		auto unknownMessages = context.aggregator().view().unknownMessages(Finalization_Point, {});

		// Assert:
		EXPECT_TRUE(unknownMessages.empty());
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsAllMessagesWhenFilterIsEmpty) {
		// Arrange:
		RunSeededAggregatorTest([](const auto& aggregator, const auto& seededShortHashes) {
			// Act:
			auto unknownMessages = aggregator.view().unknownMessages(Finalization_Point, {});

			// Assert:
			EXPECT_EQ(7u, unknownMessages.size());
			EXPECT_EQ(utils::ShortHashesSet(seededShortHashes.cbegin(), seededShortHashes.cend()), ToShortHashes(unknownMessages));
		});
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsAllMessagesNotInFilter) {
		// Arrange:
		RunSeededAggregatorTest([](const auto& aggregator, const auto& seededShortHashes) {
			// Act:
			auto unknownMessages = aggregator.view().unknownMessages(
					Finalization_Point,
					{ seededShortHashes[0], seededShortHashes[1], seededShortHashes[4], seededShortHashes[6] });

			// Assert:
			EXPECT_EQ(3u, unknownMessages.size());
			EXPECT_EQ(
					utils::ShortHashesSet({ seededShortHashes[2], seededShortHashes[3], seededShortHashes[5] }),
					ToShortHashes(unknownMessages));
		});
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsNoMessagesWhenAllMessagesAreKnown) {
		// Arrange:
		RunSeededAggregatorTest([](const auto& aggregator, const auto& seededShortHashes) {
			// Act:
			auto unknownMessages = aggregator.view().unknownMessages(
					Finalization_Point,
					utils::ShortHashesSet(seededShortHashes.cbegin(), seededShortHashes.cend()));

			// Assert:
			EXPECT_TRUE(unknownMessages.empty());
		});
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsNoMessagesWhenPointFilterDoesNotMatch) {
		// Arrange:
		for (auto point : CreateValues(Finalization_Point.unwrap(), { -2, -1, 1, 10 })) {
			RunSeededAggregatorTest([point](const auto& aggregator, const auto&) {
				// Act:
				auto unknownMessages = aggregator.view().unknownMessages(FinalizationPoint(point), {});

				// Assert:
				EXPECT_TRUE(unknownMessages.empty());
			});
		}
	}

	namespace {
		template<typename TAction>
		void RunMaxResponseSizeTests(TAction action) {
			// Arrange: determine message size from a generated message
			auto hashes = test::GenerateRandomDataVector<Hash256>(3);
			auto messageSize = CreatePrecommitMessages(1, hashes.data(), 2)[0]->Size;

			// Assert:
			action(2, 3 * messageSize - 1);
			action(3, 3 * messageSize);
			action(3, 3 * messageSize + 1);

			action(3, 4 * messageSize - 1);
			action(4, 4 * messageSize);
		}
	}

	TEST(TEST_CLASS, UnknownMessagesReturnsMessagesWithTotalSizeOfAtMostMaxResponseSize) {
		// Arrange:
		RunMaxResponseSizeTests([](uint32_t numExpectedMessages, size_t maxResponseSize) {
			TestContextOptions options;
			options.MaxResponseSize = maxResponseSize;
			TestContext context(1000, 700, options);

			auto hashes = test::GenerateRandomDataVector<Hash256>(3);
			auto messages = CreatePrecommitMessages(5, hashes.data(), 2);
			SignAllMessages(context, { 3, 1, 0, 4, 5 }, messages);

			// - add all messages and capture short hashes
			utils::ShortHashesSet seededShortHashes;
			for (const auto& pMessage : messages) {
				context.aggregator().modifier().add(pMessage);
				seededShortHashes.insert(utils::ToShortHash(model::CalculateMessageHash(*pMessage)));
			}

			// Act:
			auto unknownMessages = context.aggregator().view().unknownMessages(Finalization_Point, {});

			// Assert:
			EXPECT_EQ(numExpectedMessages, unknownMessages.size());

			// - cannot check unknownMessages exactly because there's no sorting for messages
			for (auto shortHash : ToShortHashes(unknownMessages))
				EXPECT_CONTAINS(seededShortHashes, shortHash);

			// Sanity:
			EXPECT_GT(context.aggregator().view().size(), numExpectedMessages);
		});
	}

	// endregion

	// region synchronization

	namespace {
		auto CreateLockProvider() {
			return TestContext(1000, 700).detach();
		}
	}

	DEFINE_LOCK_PROVIDER_TESTS(TEST_CLASS)

	// endregion
}}