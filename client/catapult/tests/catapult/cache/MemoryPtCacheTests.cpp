#include "catapult/cache/MemoryPtCache.h"
#include "catapult/crypto/Hashes.h"
#include "catapult/model/Cosignature.h"
#include "tests/catapult/cache/test/TransactionCacheTests.h"
#include "tests/test/core/TransactionTestUtils.h"
#include "tests/test/nodeps/LockTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace cache {

#define TEST_CLASS MemoryPtCacheTests

	// region utils

	namespace {
		constexpr auto Default_Options = MemoryCacheOptions(1'000'000, 1'000);

		void AddAll(cache::PtCache& cache, const std::vector<model::TransactionInfo>& transactionInfos) {
			auto modifier = cache.modifier();
			for (const auto& transactionInfo : transactionInfos)
				modifier.add(transactionInfo);
		}

		void AddAll(
				cache::PtCache& cache,
				const model::TransactionInfo& transactionInfo,
				const std::vector<model::Cosignature>& cosignatures) {
			auto modifier = cache.modifier();
			for (const auto& cosignature : cosignatures)
				modifier.add(transactionInfo.EntityHash, cosignature.Signer, cosignature.Signature);
		}

		auto PrepareCache(const std::vector<Hash256>& hashes, const MemoryCacheOptions& options = Default_Options) {
			auto pCache = std::make_unique<MemoryPtCache>(options);

			{
				auto i = 0u;
				auto modifier = pCache->modifier();
				for (auto& info : test::CreateTransactionInfos(hashes.size())) {
					const_cast<model::Transaction&>(*info.pEntity).Deadline = Timestamp((i + 1) * 10);
					info.EntityHash = hashes[i];
					modifier.add(info);
					++i;
				}
			}

			// Sanity:
			EXPECT_EQ(hashes.size(), pCache->view().size());
			return pCache;
		}

		auto& Sort(std::vector<model::Cosignature>& cosignatures) {
			std::sort(cosignatures.begin(), cosignatures.end(), [](const auto& lhs, const auto& rhs) { return lhs.Signer < rhs.Signer; });
			return cosignatures;
		}

		auto& Sort(std::vector<model::Cosignature>&& cosignatures) {
			return Sort(cosignatures);
		}

		void AssertCosignatures(
				const std::vector<model::Cosignature>& expectedCosignatures,
				const std::vector<model::Cosignature>& actualCosignatures,
				const std::string& message = "") {
			ASSERT_EQ(expectedCosignatures.size(), actualCosignatures.size()) << message;

			for (auto i = 0u; i < expectedCosignatures.size(); ++i) {
				auto cosignatureMessage = message + ", cosignature at " + std::to_string(i);
				const auto& expected = expectedCosignatures[i];
				const auto& actual = actualCosignatures[i];
				EXPECT_EQ(expected.Signer, actual.Signer) << cosignatureMessage;
				EXPECT_EQ(expected.Signature, actual.Signature) << cosignatureMessage;
			}
		}

		void AssertTransactionWithCosignatures(
				const model::Transaction& originalTransaction,
				const std::vector<model::Cosignature>& expectedCosignatures,
				const model::WeakCosignedTransactionInfo& transactionInfoFromCache,
				const std::string& message = "") {
			// Assert:
			ASSERT_TRUE(!!transactionInfoFromCache) << message;

			// - compare transaction
			EXPECT_EQ(originalTransaction, transactionInfoFromCache.transaction()) << message;

			// - compare cosignatures
			AssertCosignatures(expectedCosignatures, transactionInfoFromCache.cosignatures(), message);
		}
	}

	// endregion

	// region constructor

	TEST(TEST_CLASS, InitiallyCacheIsEmpty) {
		// Act:
		MemoryPtCache cache(Default_Options);

		// Assert:
		EXPECT_EQ(0u, cache.view().size());
	}

	// endregion

	// region add(transactionInfo)

	TEST(TEST_CLASS, CanAddSingleTransactionInfo) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalInfo = test::CreateRandomTransactionInfo();

		// Act:
		EXPECT_TRUE(cache.modifier().add(originalInfo));

		// Assert:
		auto view = cache.view();
		ASSERT_EQ(1u, view.size());

		auto transactionInfoFromCache = cache.view().find(originalInfo.EntityHash);
		AssertTransactionWithCosignatures(*originalInfo.pEntity, {}, transactionInfoFromCache);
	}

	TEST(TEST_CLASS, CanAddMultipleTransactionInfos) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalTransactionInfos = test::CreateTransactionInfos(5);

		// Act:
		for (const auto& transactionInfo : originalTransactionInfos)
			EXPECT_TRUE(cache.modifier().add(transactionInfo));

		// Assert:
		auto view = cache.view();
		ASSERT_EQ(5u, view.size());

		for (const auto& transactionInfo : originalTransactionInfos) {
			auto transactionInfoFromCache = cache.view().find(transactionInfo.EntityHash);
			AssertTransactionWithCosignatures(*transactionInfo.pEntity, {}, transactionInfoFromCache);
		}
	}

	TEST(TEST_CLASS, AddingSameTransactionInfosTwiceHasNoEffect) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalTransactionInfo = test::CreateRandomTransactionInfo();
		EXPECT_TRUE(cache.modifier().add(originalTransactionInfo));

		// Sanity:
		EXPECT_EQ(1u, cache.view().size());

		// Act:
		EXPECT_FALSE(cache.modifier().add(originalTransactionInfo));

		// Assert:
		auto view = cache.view();
		ASSERT_EQ(1u, view.size());

		auto transactionInfoFromCache = cache.view().find(originalTransactionInfo.EntityHash);
		AssertTransactionWithCosignatures(*originalTransactionInfo.pEntity, {}, transactionInfoFromCache);
	}

	// endregion

	// region add(cosignature)

	namespace {
		model::Cosignature GenerateRandomCosignature() {
			return { test::GenerateRandomData<Key_Size>(), test::GenerateRandomData<Signature_Size>() };
		}
	}

	TEST(TEST_CLASS, CanAttachCosignatureToKnownTransaction) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalInfos = test::CreateTransactionInfos(5);
		AddAll(cache, originalInfos);

		// Sanity:
		EXPECT_EQ(5u, cache.view().size());

		// Act:
		auto cosignature = GenerateRandomCosignature();
		auto transactionInfoFromAdd = cache.modifier().add(originalInfos[3].EntityHash, cosignature.Signer, cosignature.Signature);

		// Assert: added transaction is correct
		ASSERT_TRUE(!!transactionInfoFromAdd);
		test::AssertEqual(originalInfos[3], transactionInfoFromAdd);

		// - transaction in cache is correct
		auto transactionInfoFromCache = cache.view().find(originalInfos[3].EntityHash);
		AssertTransactionWithCosignatures(*originalInfos[3].pEntity, { cosignature }, transactionInfoFromCache);
	}

	TEST(TEST_CLASS, CanAttachManyCosignaturesToKnownTransaction) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalInfos = test::CreateTransactionInfos(5);
		AddAll(cache, originalInfos);

		// Sanity:
		EXPECT_EQ(5u, cache.view().size());

		// Act: add 20 cosignatures
		std::vector<model::Cosignature> cosignatures;
		std::vector<model::Cosignature> transactionsFromAdd;
		for (auto i = 0u; i < 20u; ++i) {
			auto cosignature = GenerateRandomCosignature();
			auto transactionInfoFromAdd = cache.modifier().add(originalInfos[3].EntityHash, cosignature.Signer, cosignature.Signature);
			cosignatures.push_back(cosignature);

			// Assert: notice that same transaction (without cosignatures) is returned by each add
			ASSERT_TRUE(!!transactionInfoFromAdd);
			test::AssertEqual(originalInfos[3], transactionInfoFromAdd);
		}

		// Assert:
		auto transactionInfoFromCache = cache.view().find(originalInfos[3].EntityHash);
		AssertTransactionWithCosignatures(*originalInfos[3].pEntity, Sort(cosignatures), transactionInfoFromCache);
	}

	TEST(TEST_CLASS, AttachingCosignatureWithSameSignerToSameTransactionTwiceHasNoEffect) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto originalInfos = test::CreateTransactionInfos(5);
		AddAll(cache, originalInfos);

		// - add a cosignature
		auto cosignature = GenerateRandomCosignature();
		EXPECT_TRUE(!!cache.modifier().add(originalInfos[3].EntityHash, cosignature.Signer, cosignature.Signature));

		// Sanity:
		EXPECT_EQ(5u, cache.view().size());

		// Act: add another cosignature with the same signer
		auto cosignature2 = GenerateRandomCosignature();
		cosignature2.Signer = cosignature.Signer;
		EXPECT_FALSE(!!cache.modifier().add(originalInfos[3].EntityHash, cosignature.Signer, cosignature.Signature));

		// Assert:
		auto transactionInfoFromCache = cache.view().find(originalInfos[3].EntityHash);
		AssertTransactionWithCosignatures(*originalInfos[3].pEntity, { cosignature }, transactionInfoFromCache);
	}

	TEST(TEST_CLASS, CannotAttachCosignatureToUnknownTransaction) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		AddAll(cache, test::CreateTransactionInfos(5));

		// Sanity:
		EXPECT_EQ(5u, cache.view().size());

		// Act + Assert: no transaction in the cache should match the random hash
		auto cosignature = GenerateRandomCosignature();
		EXPECT_FALSE(!!cache.modifier().add(test::GenerateRandomData<Hash256_Size>(), cosignature.Signer, cosignature.Signature));
	}

	// endregion

	// region remove

	TEST(TEST_CLASS, CanRemoveTransactionInfosByHash) {
		// Arrange:
		auto transactionInfos = test::CreateTransactionInfos(10);
		MemoryPtCache cache(Default_Options);
		AddAll(cache, transactionInfos);

		// Act: remove every second info
		std::vector<model::DetachedTransactionInfo> removedInfos;
		for (auto i = 0u; i < transactionInfos.size(); i += 2)
			removedInfos.push_back(cache.modifier().remove(transactionInfos[i].EntityHash));

		// Assert:
		EXPECT_EQ(5u, cache.view().size());

		// - only odd infos should remain
		for (auto i = 1u; i < transactionInfos.size(); i += 2)
			EXPECT_TRUE(!!cache.view().find(transactionInfos[i].EntityHash)) << "info at " << i;

		// - even infos should be removed
		for (auto i = 0u; i < transactionInfos.size(); i += 2) {
			EXPECT_FALSE(!!cache.view().find(transactionInfos[i].EntityHash)) << "info at " << i;
			test::AssertEqual(transactionInfos[i], removedInfos[i / 2]);
		}
	}

	TEST(TEST_CLASS, RemovingNonExistingTransactionInfosByHashHasNoEffect) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(5);
		auto pCache = PrepareCache(hashes);

		// Sanity:
		EXPECT_EQ(5u, pCache->view().size());

		// Act:
		std::vector<model::DetachedTransactionInfo> removedInfos;
		for (auto i = 0u; i < 10u; ++i)
			removedInfos.push_back(pCache->modifier().remove(test::GenerateRandomData<Hash256_Size>()));

		// Assert:
		EXPECT_EQ(5u, pCache->view().size());

		// - all hashes should remain
		for (auto i = 0u; i < hashes.size(); ++i)
			EXPECT_TRUE(!!pCache->view().find(hashes[i])) << "hash at " << i;

		// - all removed infos should be empty
		EXPECT_EQ(10u, removedInfos.size());

		auto i = 0u;
		for (const auto& removedInfo : removedInfos) {
			EXPECT_FALSE(!!removedInfo) << "removed info at " << i;
			++i;
		}
	}

	TEST(TEST_CLASS, CanAddNewTransactionInfoWithSameHashAsRemovedTransactionInfo) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);
		pCache->modifier().remove(hashes[7]);

		// Sanity:
		EXPECT_EQ(9u, pCache->view().size());
		EXPECT_FALSE(!!pCache->view().find(hashes[7]));

		// Act:
		auto newInfo = test::CreateRandomTransactionInfo();
		newInfo.EntityHash = hashes[7];
		pCache->modifier().add(std::move(newInfo));

		// Assert:
		EXPECT_EQ(10u, pCache->view().size());
		EXPECT_TRUE(!!pCache->view().find(hashes[7]));
	}

	// endregion

	// region prune (timestamp)

	TEST(TEST_CLASS, CanPruneEmptyCache_Timestamp) {
		// Arrange:
		MemoryPtCache cache(Default_Options);

		// Act: prune on empty cache does not throw
		cache.modifier().prune(Timestamp(50));

		// Assert:
		EXPECT_EQ(0u, cache.view().size());
	}

	TEST(TEST_CLASS, PruneIsNoOpIfNoPartialTransactionAreEligibleForPruning_Timestamp) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: first mosaic expires at time 10
		pCache->modifier().prune(Timestamp(9));

		// Assert: size has not changed
		auto view = pCache->view();
		EXPECT_EQ(10u, view.size());
		for (auto i = 0u; i < hashes.size(); ++i)
			EXPECT_TRUE(!!view.find(hashes[i])) << "hash at " << i;
	}

	TEST(TEST_CLASS, CanPruneSomePartialTransactions_Timestamp) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: prune transactions with deadlines [10..70]
		pCache->modifier().prune(Timestamp(70));

		// Assert: 7/10 transactions have been pruned
		auto view = pCache->view();
		EXPECT_EQ(3u, view.size());

		auto i = 0u;
		for (; i < hashes.size() - 3u; ++i)
			EXPECT_FALSE(!!view.find(hashes[i])) << "hash at " << i;

		for (; i < hashes.size(); ++i)
			EXPECT_TRUE(!!view.find(hashes[i])) << "hash at " << i;
	}

	TEST(TEST_CLASS, CanPruneAllPartialTransactions_Timestamp) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: last transaction has timestamp 110
		pCache->modifier().prune(Timestamp(150));

		// Assert: all transactions have been pruned
		EXPECT_EQ(0u, pCache->view().size());
	}

	// endregion

	// region prune (predicate)

	TEST(TEST_CLASS, CanPruneEmptyCache_Predicate) {
		// Arrange:
		MemoryPtCache cache(Default_Options);

		// Act: prune on empty cache does not throw
		cache.modifier().prune([](const auto&) { return true; });

		// Assert:
		EXPECT_EQ(0u, cache.view().size());
	}

	TEST(TEST_CLASS, PruneIsNoOpIfNoPartialTransactionAreEligibleForPruning_Predicate) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: return false from prune predicate
		pCache->modifier().prune([](const auto&) { return false; });

		// Assert: size has not changed
		auto view = pCache->view();
		EXPECT_EQ(10u, view.size());
		for (auto i = 0u; i < hashes.size(); ++i)
			EXPECT_TRUE(!!view.find(hashes[i])) << "hash at " << i;
	}

	TEST(TEST_CLASS, CanPruneSomePartialTransactions_Predicate) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: prune odd transactions
		pCache->modifier().prune([&hashes](const auto& hash) {
			auto index = std::find(hashes.cbegin(), hashes.cend(), hash) - hashes.cbegin();
			return 1 == index % 2;
		});

		// Assert: 5/10 transactions have been pruned
		auto view = pCache->view();
		EXPECT_EQ(5u, view.size());

		for (auto i = 0u; i < hashes.size(); ++i) {
			if (1 == i % 2)
				EXPECT_FALSE(!!view.find(hashes[i])) << "hash at " << i;
			else
				EXPECT_TRUE(!!view.find(hashes[i])) << "hash at " << i;
		}
	}

	TEST(TEST_CLASS, CanPruneAllPartialTransactions_Predicate) {
		// Arrange:
		auto hashes = test::GenerateRandomDataVector<Hash256>(10);
		auto pCache = PrepareCache(hashes);

		// Act: return true from prune predicate
		pCache->modifier().prune([](const auto&) { return true; });

		// Assert: all transactions have been pruned
		EXPECT_EQ(0u, pCache->view().size());
	}

	// endregion

	// region find

	namespace {
		template<typename TAction>
		void RunFindTest(TAction action) {
			// Arrange:
			MemoryPtCache cache(Default_Options);
			auto originalInfos = test::CreateTransactionInfos(10);
			AddAll(cache, originalInfos);

			// - remove every second info
			for (auto i = 0u; i < originalInfos.size(); i += 2)
				cache.modifier().remove(originalInfos[i].EntityHash);

			// Sanity:
			EXPECT_EQ(5u, cache.view().size());

			// Act + Assert:
			action(cache, originalInfos);
		}
	}

	TEST(TEST_CLASS, FindReturnsTransactionIfTransactionWithHashIsContainedInCache) {
		// Act:
		RunFindTest([](const auto& cache, const auto& originalInfos) {
			// Assert: all odd infos should still be accessible
			for (auto i = 1u; i < originalInfos.size(); i += 2) {
				auto transactionInfoFromCache = cache.view().find(originalInfos[i].EntityHash);
				AssertTransactionWithCosignatures(*originalInfos[i].pEntity, {}, transactionInfoFromCache, "info at " + std::to_string(i));
			}
		});
	}

	TEST(TEST_CLASS, FindReturnsNullptrIfTransactionWithHashIsNotContainedInCache) {
		// Act:
		RunFindTest([](const auto& cache, const auto& originalInfos) {
			// Assert: no even infos should be accessible
			for (auto i = 0u; i < originalInfos.size(); i += 2)
				EXPECT_FALSE(!!cache.view().find(originalInfos[i].EntityHash)) << "hash at " << i;
		});
	}

	// endregion

	// region shortHashPairs

	namespace {
		auto MapToShortHashes(const std::vector<model::TransactionInfo>& transactionInfos) {
			std::set<utils::ShortHash> shortHashes;
			for (const auto& transactionInfo : transactionInfos)
				shortHashes.insert(utils::ToShortHash(transactionInfo.EntityHash));

			return shortHashes;
		}

		template<typename TShortHashSupplier>
		void ValidateShortHashPairs(
				const std::vector<model::TransactionInfo>& transactionInfos,
				const ShortHashPairRange& shortHashPairs,
				TShortHashSupplier getExpectedCosignaturesShortHash) {
			auto expectedShortHashes = MapToShortHashes(transactionInfos);

			// Assert:
			ASSERT_EQ(expectedShortHashes.size(), shortHashPairs.size());

			auto i = 0u;
			for (const auto& shortHashPair : shortHashPairs) {
				auto message = "at index " + std::to_string(i);
				auto expectedShortHashIter = expectedShortHashes.find(shortHashPair.TransactionShortHash);
				EXPECT_NE(expectedShortHashes.cend(), expectedShortHashIter) << message;
				EXPECT_EQ(getExpectedCosignaturesShortHash(*expectedShortHashIter), shortHashPair.CosignaturesShortHash) << message;

				expectedShortHashes.erase(expectedShortHashIter);
				++i;
			}

			// Sanity:
			EXPECT_TRUE(expectedShortHashes.empty());
		}

		Hash256 HashCosignatures(const std::vector<model::Cosignature>& cosignatures) {
			Hash256 cosignaturesHash;
			crypto::Sha3_256(
					{ reinterpret_cast<const uint8_t*>(cosignatures.data()), cosignatures.size() * sizeof(model::Cosignature) },
					cosignaturesHash);
			return cosignaturesHash;
		}
	}

	TEST(TEST_CLASS, ShortHashesReturnsAllShortHashesForTransactionsWithoutCosignatures) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto transactionInfos = test::CreateTransactionInfos(10);
		AddAll(cache, transactionInfos);

		// Act:
		auto shortHashPairs = cache.view().shortHashPairs();

		// Assert: all cosignatures short hashes should be zeroed
		ValidateShortHashPairs(transactionInfos, shortHashPairs, [](const auto&) { return utils::ShortHash(); });
	}

	TEST(TEST_CLASS, ShortHashesReturnAppropriateShortHashForTransactionWithCosignatures) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto transactionInfos = test::CreateTransactionInfos(3);
		AddAll(cache, transactionInfos);

		// - generate and sort cosignatures and add them to a transaction
		auto cosignatures = Sort(test::GenerateRandomDataVector<model::Cosignature>(10));
		AddAll(cache, transactionInfos[1], cosignatures);

		// - calculate the expected cosignatures hash
		auto expectedCosignaturesHash = HashCosignatures(cosignatures);

		// Act:
		auto shortHashPairs = cache.view().shortHashPairs();

		// Assert:
		ValidateShortHashPairs(
				transactionInfos,
				shortHashPairs,
				[&expectedCosignaturesHash, &targetEntityHash = transactionInfos[1].EntityHash](const auto& transactionShortHash) {
			return utils::ToShortHash(targetEntityHash) == transactionShortHash
					? utils::ToShortHash(expectedCosignaturesHash)
					: utils::ShortHash();
		});
	}

	TEST(TEST_CLASS, ShortHashesReturnOrderIndependentCosignaturesShortHash) {
		// Arrange:
		MemoryPtCache cache(Default_Options);
		auto transactionInfos = test::CreateTransactionInfos(3);
		AddAll(cache, transactionInfos);

		// - generate cosignatures and add them to all transactions
		auto cosignatures = test::GenerateRandomDataVector<model::Cosignature>(10);
		for (auto i = 0u; i < cosignatures.size(); ++i) {
			for (auto j = 0u; j < transactionInfos.size(); ++j) {
				const auto& cosignature = cosignatures[(i + j) % cosignatures.size()];
				cache.modifier().add(transactionInfos[j].EntityHash, cosignature.Signer, cosignature.Signature);
			}
		}

		// - calculate the expected cosignatures hash (notice that the cosignatures must be sorted)
		auto expectedCosignaturesHash = HashCosignatures(Sort(cosignatures));

		// Act:
		auto shortHashPairs = cache.view().shortHashPairs();

		// Assert:
		ValidateShortHashPairs(transactionInfos, shortHashPairs, [&expectedCosignaturesHash](const auto&) {
			return utils::ToShortHash(expectedCosignaturesHash);
		});
	}

	// endregion

	// region unknownTransactions - helpers

	namespace {
		using UnknownTransactionInfos = std::vector<model::CosignedTransactionInfo>;
		using UnknownTransactionInfoMap = std::unordered_map<Hash256, model::CosignedTransactionInfo, utils::ArrayHasher<Hash256>>;

		UnknownTransactionInfoMap ToMap(const UnknownTransactionInfos& unknownTransactionInfos) {
			UnknownTransactionInfoMap infoMap;
			for (const auto& unknownTransactionInfo : unknownTransactionInfos)
				infoMap.emplace(unknownTransactionInfo.EntityHash, unknownTransactionInfo);

			return infoMap;
		}
	}

	// endregion

	// region unknownTransactions - basic (no cosignatures)

	namespace {
		struct MemoryPtCacheUnknownTransactionsTraits {
		public:
			using CacheType = MemoryPtCache;

		public:
			static void AddAllToCache(cache::PtCache& cache, const std::vector<model::TransactionInfo>& transactionInfos) {
				AddAll(cache, transactionInfos);
			}

			static void AssertUnknownResult(
					const std::vector<model::TransactionInfo>& expectedTransactionInfos,
					const std::vector<model::CosignedTransactionInfo>& unknownInfos) {
				auto unknownInfoMap = ToMap(unknownInfos);

				for (const auto& transactionInfo : expectedTransactionInfos) {
					auto iter = unknownInfoMap.find(transactionInfo.EntityHash);
					ASSERT_NE(unknownInfoMap.cend(), iter);

					EXPECT_EQ(transactionInfo.EntityHash, iter->second.EntityHash);
					EXPECT_EQ(transactionInfo.pEntity, iter->second.pTransaction);
					EXPECT_TRUE(iter->second.Cosignatures.empty());

					unknownInfoMap.erase(iter);
				}

				// all infos were found
				EXPECT_TRUE(unknownInfoMap.empty());
			}

			static auto MapToFilterId(const model::TransactionInfo& transactionInfo) {
				// only transaction short hash is relevant in basic tests
				return std::make_pair(utils::ToShortHash(transactionInfo.EntityHash), utils::ShortHash());
			}
		};
	}

	DEFINE_BASIC_UNKNOWN_TRANSACTIONS_TESTS(MemoryPtCacheTests, MemoryPtCacheUnknownTransactionsTraits);

	// endregion

	// region unknownTransactions - cosignatures

	namespace {
		template<typename TAction>
		void RunUnknownTransactionWithCosignaturesTest(TAction action) {
			// Arrange:
			MemoryPtCache cache(Default_Options);
			auto transactionInfos = test::CreateTransactionInfos(1);
			AddAll(cache, transactionInfos);

			// - generate and sort cosignatures and add them to a transaction
			auto cosignatures = Sort(test::GenerateRandomDataVector<model::Cosignature>(5));
			AddAll(cache, transactionInfos[0], cosignatures);

			// - calculate the cosignatures hash
			auto expectedCosignaturesHash = HashCosignatures(cosignatures);

			// Act + Assert:
			auto shortHashPair = ShortHashPair{
				utils::ToShortHash(transactionInfos[0].EntityHash),
				utils::ToShortHash(expectedCosignaturesHash)
			};
			action(cache, transactionInfos[0], cosignatures, shortHashPair);
		}
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsTransactionAndCosignaturesIfTransactionIsUnknown) {
		// Arrange:
		RunUnknownTransactionWithCosignaturesTest([](const auto& cache, const auto& info, const auto& cosignatures, auto) {
			// Act:
			auto unknownInfos = cache.view().unknownTransactions({});

			// Assert:
			ASSERT_EQ(1u, unknownInfos.size());
			EXPECT_EQ(info.EntityHash, unknownInfos[0].EntityHash);
			EXPECT_EQ(info.pEntity, unknownInfos[0].pTransaction);
			AssertCosignatures(cosignatures, unknownInfos[0].Cosignatures);
		});
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsOnlyCosignaturesIfTransactionIsKnownButHasDifferentCosignatures) {
		// Arrange:
		RunUnknownTransactionWithCosignaturesTest([](const auto& cache, const auto& info, const auto& cosignatures, auto shortHashPair) {
			// Act:
			auto unknownInfos = cache.view().unknownTransactions({
				{ shortHashPair.TransactionShortHash, utils::ShortHash() }
			});

			// Assert:
			ASSERT_EQ(1u, unknownInfos.size());
			EXPECT_EQ(info.EntityHash, unknownInfos[0].EntityHash);
			EXPECT_FALSE(!!unknownInfos[0].pTransaction);
			AssertCosignatures(cosignatures, unknownInfos[0].Cosignatures);
		});
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsNothingIfTransactionAndCosignaturesBothMatch) {
		// Arrange:
		RunUnknownTransactionWithCosignaturesTest([](const auto& cache, const auto&, const auto&, auto shortHashPair) {
			// Act:
			auto unknownInfos = cache.view().unknownTransactions({
				{ shortHashPair.TransactionShortHash, shortHashPair.CosignaturesShortHash }
			});

			// Assert:
			EXPECT_TRUE(unknownInfos.empty());
		});
	}

	// endregion

	// region unknownTransactions - max response size

	namespace {
		uint32_t GetTransactionSize() {
			// determine transaction size from a generated transaction
			return test::CreateTransactionInfos(1)[0].pEntity->Size;
		}

		uint64_t TotalSize(const UnknownTransactionInfos& unknownTransactionInfos) {
			uint64_t totalSize = 0u;
			for (const auto& unknownTransactionInfo : unknownTransactionInfos) {
				auto transactionSize = unknownTransactionInfo.pTransaction ? unknownTransactionInfo.pTransaction->Size : 0;
				totalSize += transactionSize + sizeof(model::Cosignature) * unknownTransactionInfo.Cosignatures.size();
			}

			return totalSize;
		}

		void AssertMaxResponseSizeIsRespectedOnlyTransactions(uint32_t numExpectedTransactions, uint32_t maxResponseSize) {
			// Arrange:
			MemoryPtCache cache(MemoryCacheOptions(maxResponseSize, 1000));
			AddAll(cache, test::CreateTransactionInfos(5));

			// Act:
			auto unknownInfos = cache.view().unknownTransactions({});

			// Assert: notice that no ordering is guaranteed
			EXPECT_EQ(numExpectedTransactions, unknownInfos.size());
			EXPECT_GE(maxResponseSize, TotalSize(unknownInfos));
		}

		void AssertMaxResponseSizeIsRespectedOnlyCosignatures(uint32_t numExpectedTransactions, uint32_t maxResponseSize) {
			// Arrange:
			MemoryPtCache cache(MemoryCacheOptions(maxResponseSize, 1000));
			auto transactionInfos = test::CreateTransactionInfos(5);
			AddAll(cache, transactionInfos);

			// - mark all transactions as known (with different cosignatures) and add three cosignatures per info
			ShortHashPairMap knownShortHashes;
			for (const auto& transactionInfo : transactionInfos) {
				knownShortHashes.emplace(utils::ToShortHash(transactionInfo.EntityHash), utils::ShortHash());
				AddAll(cache, transactionInfo, test::GenerateRandomDataVector<model::Cosignature>(3));
			}

			// Act:
			auto unknownInfos = cache.view().unknownTransactions(knownShortHashes);

			// Assert: notice that no ordering is guaranteed
			EXPECT_EQ(numExpectedTransactions, unknownInfos.size());
			EXPECT_GE(maxResponseSize, TotalSize(unknownInfos));
		}

		void AssertMaxResponseSizeIsRespectedTransactionsWithCosignatures(uint32_t numExpectedTransactions, uint32_t maxResponseSize) {
			// Arrange:
			MemoryPtCache cache(MemoryCacheOptions(maxResponseSize, 1000));
			auto transactionInfos = test::CreateTransactionInfos(5);
			AddAll(cache, transactionInfos);

			// - add three cosignatures per info
			for (const auto& transactionInfo : transactionInfos)
				AddAll(cache, transactionInfo, test::GenerateRandomDataVector<model::Cosignature>(3));

			// Act:
			auto unknownInfos = cache.view().unknownTransactions({});

			// Assert: notice that no ordering is guaranteed
			EXPECT_EQ(numExpectedTransactions, unknownInfos.size());
			EXPECT_GE(maxResponseSize, TotalSize(unknownInfos));
		}

		template<typename TAssertMaxResponseSize>
		void RunMaxResponseSizeTest(uint32_t transactionSize, TAssertMaxResponseSize assertMaxResponseSize) {
			// Assert:
			assertMaxResponseSize(2, 3 * transactionSize - 1);
			assertMaxResponseSize(3, 3 * transactionSize);
			assertMaxResponseSize(3, 3 * transactionSize + 1);

			assertMaxResponseSize(3, 4 * transactionSize - 1);
			assertMaxResponseSize(4, 4 * transactionSize);
		}
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsTransactionsWithTotalSizeOfAtMostMaxResponseSize_OnlyTransactions) {
		// Assert:
		RunMaxResponseSizeTest(sizeof(Hash256) + GetTransactionSize(), AssertMaxResponseSizeIsRespectedOnlyTransactions);
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsTransactionsWithTotalSizeOfAtMostMaxResponseSize_OnlyCosignatures) {
		// Assert:
		RunMaxResponseSizeTest(sizeof(Hash256) + 3 * sizeof(model::Cosignature), AssertMaxResponseSizeIsRespectedOnlyCosignatures);
	}

	TEST(TEST_CLASS, UnknownTransactionsReturnsTransactionsWithTotalSizeOfAtMostMaxResponseSize_TransactionsWithCosignatures) {
		// Assert:
		RunMaxResponseSizeTest(
				sizeof(Hash256) + GetTransactionSize() + 3 * sizeof(model::Cosignature),
				AssertMaxResponseSizeIsRespectedTransactionsWithCosignatures);
	}

	// endregion

	// region max size

	TEST(TEST_CLASS, CacheCanContainMaxTransactions) {
		// Arrange: fill the cache with one less than max transactions
		MemoryPtCache cache(MemoryCacheOptions(1024, 5));
		AddAll(cache, test::CreateTransactionInfos(4));
		auto transactionInfo = test::CreateRandomTransactionInfo();

		// Act: add another info
		auto isAdded = cache.modifier().add(transactionInfo);

		// Assert: the new info was added
		EXPECT_TRUE(isAdded);
		EXPECT_EQ(5u, cache.view().size());
		EXPECT_TRUE(!!cache.view().find(transactionInfo.EntityHash));
	}

	TEST(TEST_CLASS, CacheCannotContainMoreThanMaxTransactions) {
		// Arrange: fill the cache with max transactions
		MemoryPtCache cache(MemoryCacheOptions(1024, 5));
		AddAll(cache, test::CreateTransactionInfos(5));
		auto transactionInfo = test::CreateRandomTransactionInfo();

		// Act: add another info
		auto isAdded = cache.modifier().add(transactionInfo);

		// Assert: the new info was not added
		EXPECT_FALSE(isAdded);
		EXPECT_EQ(5u, cache.view().size());
		EXPECT_FALSE(!!cache.view().find(transactionInfo.EntityHash));
	}

	TEST(TEST_CLASS, CacheCanAcceptNewTransactionsAfterMaxTransactionsAreReduced) {
		// Arrange:
		MemoryPtCache cache(MemoryCacheOptions(1024, 5));
		auto transactionInfo = test::CreateRandomTransactionInfo();

		// - fill the cache with max transactions
		auto seedTransactionInfos = test::CreateTransactionInfos(5);
		auto seedHash = seedTransactionInfos[2].EntityHash;
		AddAll(cache, seedTransactionInfos);

		// Act: remove a transaction from the cache and add a new transaction
		auto isAdded = false;
		{
			auto modifier = cache.modifier();
			modifier.remove(seedHash);
			isAdded = modifier.add(transactionInfo);
		}

		// Assert: the new info was added
		EXPECT_TRUE(isAdded);
		EXPECT_EQ(5u, cache.view().size());
		EXPECT_TRUE(!!cache.view().find(transactionInfo.EntityHash));
	}

	// endregion

	// region synchronization

	namespace {
		auto CreateLockProvider() {
			return std::make_unique<MemoryPtCache>(Default_Options);
		}
	}

	DEFINE_LOCK_PROVIDER_TESTS(MemoryPtCacheTests)

	// endregion
}}
