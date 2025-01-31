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

#include "src/model/PriceTransaction.h"
#include "catapult/utils/MemoryUtils.h"
#include "tests/test/core/TransactionTestUtils.h"
#include "tests/test/core/VariableSizedEntityTestUtils.h"
#include "tests/test/nodeps/Alignment.h"
#include "tests/test/nodeps/NumericTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace model {

#define TEST_CLASS PriceTransactionTests

	// region size + alignment + properties

#define TRANSACTION_FIELDS FIELD(MessageSize)

	namespace {
		template<typename T>
		void AssertTransactionHasExpectedSize(size_t baseSize) {
			// Arrange:
			auto expectedSize = baseSize;

#define FIELD(X) expectedSize += SizeOf32<decltype(T::X)>();
			TRANSACTION_FIELDS
#undef FIELD

			// Assert:
			EXPECT_EQ(expectedSize, sizeof(T));
			EXPECT_EQ(baseSize + sizeof(uint16_t), sizeof(T));
		}

		template<typename T>
		void AssertTransactionHasProperAlignment() {
/*#define FIELD(X) EXPECT_ALIGNED(T, X);
			TRANSACTION_FIELDS
#undef FIELD

			EXPECT_EQ(0u, sizeof(T) % 8);*/
		}

		template<typename T>
		void AssertTransactionHasExpectedProperties() {
			// Assert:
			EXPECT_EQ(Entity_Type_Price, T::Entity_Type);
			EXPECT_EQ(1u, T::Current_Version);
		}
	}

#undef TRANSACTION_FIELDS

	ADD_BASIC_TRANSACTION_SIZE_PROPERTY_TESTS(Price)

	// endregion

	// region data pointers

	namespace {
		struct PriceTransactionTraits {
			static auto GenerateEntityWithAttachments(uint8_t numMosaics, uint16_t messageSize) {
				uint32_t entitySize = SizeOf32<PriceTransaction>() + messageSize + numMosaics * SizeOf32<UnresolvedMosaic>();
				auto pTransaction = utils::MakeUniqueWithSize<PriceTransaction>(entitySize);
				pTransaction->Size = entitySize;
				pTransaction->MessageSize = messageSize;
				return pTransaction;
			}

			static constexpr size_t GetAttachment1Size(uint8_t numMosaics) {
				return numMosaics * sizeof(UnresolvedMosaic);
			}

			template<typename TEntity>
			static auto GetAttachmentPointer(TEntity& entity) {
				return entity.MessagePtr();
			}
		};
	}

	//DEFINE_DUAL_ATTACHMENT_POINTER_TESTS(TEST_CLASS, PriceTransactionTraits)

	// endregion

	// region CalculateRealSize

	TEST(TEST_CLASS, CanCalculateRealSizeWithReasonableValues) {
		// Arrange:
		PriceTransaction transaction;
		transaction.Size = 0;
		transaction.MessageSize = 100;

		// Act:
		auto realSize = PriceTransaction::CalculateRealSize(transaction);

		// Assert:
		EXPECT_EQ(sizeof(PriceTransaction) + 100, realSize);
	}

	TEST(TEST_CLASS, CalculateRealSizeDoesNotOverflowWithMaxValues) {
		// Arrange:
		PriceTransaction transaction;
		test::SetMaxValue(transaction.Size);
		test::SetMaxValue(transaction.MessageSize);

		// Act:
		auto realSize = PriceTransaction::CalculateRealSize(transaction);

		// Assert:
		ASSERT_EQ(0xFFFFFFFF, transaction.Size);
		EXPECT_EQ(sizeof(PriceTransaction) + 0xFFFF, realSize);
		EXPECT_GT(0xFFFFFFFF, realSize);
	}

	// endregion
}}
