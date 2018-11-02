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

#include "catapult/cache/PatriciaTreeUtils.h"
#include "tests/catapult/cache/test/PatriciaTreeTestUtils.h"
#include "tests/test/other/DeltaElementsTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace cache {

#define TEST_CLASS PatriciaTreeUtilsTests

	namespace {
		using MemoryPatriciaTree = test::MemoryPatriciaTree;
		using DeltasWrapper = test::DeltaElementsTestUtils::WrapperWithGenerationalSupport<std::unordered_map<uint32_t, std::string>>;

		Hash256 CalculateRootHashForTreeWithFourNodes() {
			return test::CalculateRootHash({
				{ 0x64'6F'00'00, "verb" },
				{ 0x64'6F'67'00, "puppy" },
				{ 0x64'6F'67'65, "coin" },
				{ 0x68'6F'72'73, "stallion" }
			});
		}
	}

	// region zero operations

	TEST(TEST_CLASS, TreeRootIsUnchangedWhenDeltasAreEmpty) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = CalculateRootHashForTreeWithFourNodes();

		EXPECT_EQ(expectedRoot, tree.root());
	}

	// endregion

	// region single operations

	TEST(TEST_CLASS, DeltaAdditionsCanBeAppliedToTree) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Added.emplace(0x26'54'32'10, "alpha");
		deltaset.Added.emplace(0x46'54'32'10, "beta");

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "verb" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "coin" },
			{ 0x68'6F'72'73, "stallion" },
			{ 0x26'54'32'10, "alpha" },
			{ 0x46'54'32'10, "beta" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, DeltaRemovalsCanBeAppliedToTree) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Removed.emplace(0x64'6F'00'00, "verb");
		deltaset.Removed.emplace(0x64'6F'67'65, "coin");

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'67'00, "puppy" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, DeltaCopiesCanBeAppliedToTree) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Copied.emplace(0x64'6F'00'00, "noun");
		deltaset.Copied.emplace(0x64'6F'67'65, "bill");

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "noun" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "bill" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	// endregion

	// region single operations - height dependent

	namespace {
		class HeightDependentValue {
		public:
			HeightDependentValue(const std::string& value, Height height)
					: m_value(value)
					, m_height(height)
			{}

		public:
			const std::string& str() const {
				return m_value;
			}

			bool isActive(Height height) const {
				return height <= m_height;
			}

		private:
			std::string m_value;
			Height m_height;
		};

		struct HeightDependentValueSimpleSerializer {
			using KeyType = uint32_t;
			using ValueType = HeightDependentValue;

			static const std::string& SerializeValue(const ValueType& value) {
				return value.str();
			}
		};

		using HeightDependentDeltasWrapper = test::DeltaElementsTestUtils::WrapperWithGenerationalSupport<
			std::unordered_map<uint32_t, HeightDependentValue>>;

		using HeightDependentMemoryPatriciaTree = tree::PatriciaTree<
			cache::SerializerPlainKeyEncoder<HeightDependentValueSimpleSerializer>,
			tree::MemoryDataSource>;

		void SeedTreeWithFourNodes(HeightDependentMemoryPatriciaTree& tree) {
			tree.set(0x64'6F'00'00, HeightDependentValue("verb", Height(30)));
			tree.set(0x64'6F'67'00, HeightDependentValue("puppy", Height(40)));
			tree.set(0x64'6F'67'65, HeightDependentValue("coin", Height(50)));
			tree.set(0x68'6F'72'73, HeightDependentValue("stallion", Height(60)));
		}
	}

	TEST(TEST_CLASS, DeltaAdditionsCanBeAppliedToTree_Active) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		HeightDependentMemoryPatriciaTree tree(dataSource);
		SeedTreeWithFourNodes(tree);

		HeightDependentDeltasWrapper deltaset;
		deltaset.Added.emplace(0x26'54'32'10, HeightDependentValue("alpha", Height(70)));
		deltaset.Added.emplace(0x46'54'32'10, HeightDependentValue("beta", Height(80)));

		// Act: added elements are active at height 20
		ApplyDeltasToTree(tree, deltaset, 1, Height(20));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "verb" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "coin" },
			{ 0x68'6F'72'73, "stallion" },
			{ 0x26'54'32'10, "alpha" },
			{ 0x46'54'32'10, "beta" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, DeltaAdditionsCannotBeAppliedToTree_Inactive) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		HeightDependentMemoryPatriciaTree tree(dataSource);
		SeedTreeWithFourNodes(tree);

		HeightDependentDeltasWrapper deltaset;
		deltaset.Added.emplace(0x26'54'32'10, HeightDependentValue("alpha", Height(70)));
		deltaset.Added.emplace(0x46'54'32'10, HeightDependentValue("beta", Height(80)));

		// Act: added elements are inactive at height 100
		EXPECT_THROW(ApplyDeltasToTree(tree, deltaset, 1, Height(100)), catapult_runtime_error);
	}

	TEST(TEST_CLASS, DeltaCopiesCanBeAppliedToTree_Active) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		HeightDependentMemoryPatriciaTree tree(dataSource);
		SeedTreeWithFourNodes(tree);

		HeightDependentDeltasWrapper deltaset;
		deltaset.Copied.emplace(0x64'6F'00'00, HeightDependentValue("noun", Height(70)));
		deltaset.Copied.emplace(0x64'6F'67'65, HeightDependentValue("bill", Height(80)));

		// Act: copied elements are active at height 20
		ApplyDeltasToTree(tree, deltaset, 1, Height(20));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "noun" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "bill" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, DeltaCopiesCanBeAppliedToTree_Inactive) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		HeightDependentMemoryPatriciaTree tree(dataSource);
		SeedTreeWithFourNodes(tree);

		HeightDependentDeltasWrapper deltaset;
		deltaset.Copied.emplace(0x64'6F'00'00, HeightDependentValue("noun", Height(70)));
		deltaset.Copied.emplace(0x64'6F'67'65, HeightDependentValue("bill", Height(80)));

		// Act: copied elements are inactive at height 100
		ApplyDeltasToTree(tree, deltaset, 1, Height(100));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'67'00, "puppy" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, DeltaCopiesCanBeAppliedToTree_MixedActiveInactive) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		HeightDependentMemoryPatriciaTree tree(dataSource);
		SeedTreeWithFourNodes(tree);

		HeightDependentDeltasWrapper deltaset;
		deltaset.Copied.emplace(0x64'6F'00'00, HeightDependentValue("noun", Height(70)));
		deltaset.Copied.emplace(0x64'6F'67'65, HeightDependentValue("bill", Height(80)));

		// Act: only one copied element is active at height 75
		ApplyDeltasToTree(tree, deltaset, 1, Height(75));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "bill" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	// endregion

	// region multiple operations

	TEST(TEST_CLASS, AllDeltaChangesAreAppliedToTree) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Added.emplace(0x26'54'32'10, "alpha");
		deltaset.Removed.emplace(0x64'6F'67'65, "coin");
		deltaset.Copied.emplace(0x64'6F'00'00, "noun");

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "noun" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x68'6F'72'73, "stallion" },
			{ 0x26'54'32'10, "alpha" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, AllDeltaChangesAreAppliedToTreeDeterministically) {
		// Arrange: added < copied < removed
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Added.emplace(0x26'54'32'10, "pug");
		deltaset.Copied.emplace(0x26'54'32'10, "terrier");
		deltaset.Removed.emplace(0x26'54'32'10, "terrier");

		deltaset.Copied.emplace(0x64'6F'00'00, "noun");
		deltaset.Removed.emplace(0x64'6F'00'00, "noun");

		deltaset.Added.emplace(0x46'54'32'10, "lion");
		deltaset.Copied.emplace(0x46'54'32'10, "tiger");

		deltaset.Added.emplace(0x46'98'21'44, "bison");

		// Act:
		ApplyDeltasToTree(tree, deltaset, 1, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "coin" },
			{ 0x68'6F'72'73, "stallion" },
			{ 0x46'54'32'10, "tiger" },
			{ 0x46'98'21'44, "bison" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	// endregion

	// region generations

	TEST(TEST_CLASS, OnlyGenerationChangesMatchingCurrentGenerationAreApplied_Added) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		// - prepare two adds for each generation because adds are independent of existing elements
		//   and this provides a bit better evidence of proper looping and filtering
		DeltasWrapper deltaset;

		auto values = std::vector<std::string>{ "one", "two", "three", "four", "five", "six", "seven", "eight" };
		for (uint8_t i = 0; i < values.size(); ++i) {
			deltaset.Added.emplace(0x26'54'32'00 + i, values[i]);
			deltaset.setGenerationId(0x26'54'32'00 + i, (i % 4) + 1);
		}

		deltaset.incrementGenerationId();
		deltaset.incrementGenerationId(); // active generation id is 3

		// Act:
		ApplyDeltasToTree(tree, deltaset, 2, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "verb" },
			{ 0x64'6F'67'00, "puppy" },
			{ 0x64'6F'67'65, "coin" },
			{ 0x68'6F'72'73, "stallion" },

			{ 0x26'54'32'01, "two" },
			{ 0x26'54'32'02, "three" },
			{ 0x26'54'32'05, "six" },
			{ 0x26'54'32'06, "seven" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, OnlyGenerationChangesMatchingCurrentGenerationAreApplied_Copied) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Copied.emplace(0x64'6F'00'00, "noun");
		deltaset.Copied.emplace(0x64'6F'67'00, "kitten");
		deltaset.Copied.emplace(0x64'6F'67'65, "bill");
		deltaset.Copied.emplace(0x68'6F'72'73, "pony");

		deltaset.setGenerationId(0x64'6F'00'00, 1);
		deltaset.setGenerationId(0x64'6F'67'00, 2);
		deltaset.setGenerationId(0x64'6F'67'65, 3);
		deltaset.setGenerationId(0x68'6F'72'73, 4);

		deltaset.incrementGenerationId();
		deltaset.incrementGenerationId(); // active generation id is 3

		// Act:
		ApplyDeltasToTree(tree, deltaset, 2, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "verb" },
			{ 0x64'6F'67'00, "kitten" },
			{ 0x64'6F'67'65, "bill" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	TEST(TEST_CLASS, OnlyGenerationChangesMatchingCurrentGenerationAreApplied_Removed) {
		// Arrange:
		tree::MemoryDataSource dataSource;
		MemoryPatriciaTree tree(dataSource);
		test::SeedTreeWithFourNodes(tree);

		DeltasWrapper deltaset;
		deltaset.Removed.emplace(0x64'6F'00'00, "verb");
		deltaset.Removed.emplace(0x64'6F'67'00, "puppy");
		deltaset.Removed.emplace(0x64'6F'67'65, "coin");
		deltaset.Removed.emplace(0x68'6F'72'73, "stallion");

		deltaset.setGenerationId(0x64'6F'00'00, 1);
		deltaset.setGenerationId(0x64'6F'67'00, 2);
		deltaset.setGenerationId(0x64'6F'67'65, 3);
		deltaset.setGenerationId(0x68'6F'72'73, 4);

		deltaset.incrementGenerationId();
		deltaset.incrementGenerationId(); // active generation id is 3

		// Act:
		ApplyDeltasToTree(tree, deltaset, 2, Height(1));

		// Assert:
		auto expectedRoot = test::CalculateRootHash({
			{ 0x64'6F'00'00, "verb" },
			{ 0x68'6F'72'73, "stallion" }
		});

		EXPECT_EQ(expectedRoot, tree.root());
	}

	// endregion
}}
