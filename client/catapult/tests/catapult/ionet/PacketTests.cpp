#include "catapult/ionet/Packet.h"
#include "tests/TestHarness.h"

namespace catapult { namespace ionet {

#define TEST_CLASS PacketTests

	namespace {
		constexpr auto Test_Packet_Type = static_cast<PacketType>(987);

#pragma pack(push, 1)

		struct TestPacket : public Packet {
			static constexpr PacketType Packet_Type = Test_Packet_Type;

			uint32_t Foo;
			uint8_t Bar;
			uint16_t Baz;
		};

#pragma pack(pop)
	}

	// region CreateSharedPacket

	TEST(TEST_CLASS, CanCreateSharedPacketOfBaseType) {
		// Act:
		auto pPacket = CreateSharedPacket<Packet>();

		// Assert:
		EXPECT_EQ(8u, pPacket->Size);
		EXPECT_EQ(PacketType::Undefined, pPacket->Type);
	}

	TEST(TEST_CLASS, CanCreateSharedPacketOfBaseTypeWithPayload) {
		// Act:
		auto pPacket = CreateSharedPacket<Packet>(1234);

		// Assert:
		EXPECT_EQ(1242u, pPacket->Size);
		EXPECT_EQ(PacketType::Undefined, pPacket->Type);
	}

	TEST(TEST_CLASS, CanCreateSharedPacketOfDerivedType) {
		// Act:
		auto pPacket = CreateSharedPacket<TestPacket>();

		// Assert:
		EXPECT_EQ(15u, pPacket->Size);
		EXPECT_EQ(Test_Packet_Type, pPacket->Type);
	}

	TEST(TEST_CLASS, CanCreateSharedPacketOfDerivedTypeWithPayload) {
		// Act:
		auto pPacket = CreateSharedPacket<TestPacket>(1234);

		// Assert:
		EXPECT_EQ(1249u, pPacket->Size);
		EXPECT_EQ(Test_Packet_Type, pPacket->Type);
	}

	// endregion

	// region Data

	namespace {
		struct ConstTraits {
			using BytePointerType = const uint8_t*;

			static BytePointerType GetData(const Packet& packet) {
				return packet.Data();
			}
		};

		struct NonConstTraits {
			using BytePointerType = uint8_t*;

			static BytePointerType GetData(Packet& packet) {
				return packet.Data();
			}
		};
	}

#define DATA_TEST(TEST_NAME) \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)(); \
	TEST(TEST_CLASS, TEST_NAME##_Const) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<ConstTraits>(); } \
	TEST(TEST_CLASS, TEST_NAME##_NonConst) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<NonConstTraits>(); } \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)()

	DATA_TEST(DataAreInaccessibleWhenReportedSizeIsLessThanHeaderSize) {
		// Arrange:
		auto pPacket = CreateSharedPacket<Packet>();
		pPacket->Size = sizeof(Packet) - 1;

		// Act + Assert:
		EXPECT_FALSE(!!TTraits::GetData(*pPacket));
	}

	DATA_TEST(DataAreInaccessibleWhenReportedSizeIsEqualToHeaderSize) {
		// Arrange:
		auto pPacket = CreateSharedPacket<Packet>();

		// Act + Assert:
		EXPECT_FALSE(!!TTraits::GetData(*pPacket));
	}

	DATA_TEST(DataAreAccessibleWhenReportedSizeIsGreaterThanHeaderSize) {
		// Arrange:
		auto pPacket = CreateSharedPacket<TestPacket>();
		auto pExpectedPacketData = reinterpret_cast<typename TTraits::BytePointerType>(pPacket.get()) + sizeof(Packet);

		// Act + Assert:
		auto pActualPacketData = TTraits::GetData(*pPacket);
		EXPECT_TRUE(!!pActualPacketData);
		EXPECT_EQ(pExpectedPacketData, pActualPacketData);
	}

	// endregion

	// region CoercePacket

	namespace {
		void AssertCannotCoercePacket(const consumer<Packet&>& modifyPacket) {
			// Arrange:
			auto pPacket = CreateSharedPacket<TestPacket>();
			modifyPacket(*pPacket);

			// Act:
			const auto* pCoercedPacket = CoercePacket<TestPacket>(pPacket.get());

			// Assert:
			EXPECT_FALSE(!!pCoercedPacket);
		}
	}

	TEST(TEST_CLASS, CannotCoercePacketWithWrongType) {
		// Assert:
		AssertCannotCoercePacket([](auto& packet) { packet.Type = PacketType::Push_Block; });
	}

	TEST(TEST_CLASS, CannotCoercePacketWithSizeTooSmall) {
		// Assert:
		AssertCannotCoercePacket([](auto& packet) { --packet.Size; });
	}

	TEST(TEST_CLASS, CannotCoercePacketWithSizeTooLarge) {
		// Assert:
		AssertCannotCoercePacket([](auto& packet) { ++packet.Size; });
	}

	TEST(TEST_CLASS, CanCoercePacketWithCorrectTypeAndSize) {
		// Arrange:
		auto pPacket = CreateSharedPacket<TestPacket>();

		// Act:
		const auto* pCoercedPacket = CoercePacket<TestPacket>(pPacket.get());

		// Assert:
		EXPECT_TRUE(!!pCoercedPacket);
	}

	// endregion

	// region IsPacketValid

	namespace {
		void AssertPacketIsNotValid(const consumer<Packet&>& modifyPacket) {
			// Arrange:
			auto pPacket = CreateSharedPacket<Packet>();
			pPacket->Type = Test_Packet_Type;
			modifyPacket(*pPacket);

			// Act:
			auto isValid = IsPacketValid(*pPacket, Test_Packet_Type);

			// Assert:
			EXPECT_FALSE(isValid);
		}
	}

	TEST(TEST_CLASS, PacketWithWrongTypeIsNotValid) {
		// Assert:
		AssertPacketIsNotValid([](auto& packet) { packet.Type = PacketType::Push_Block; });
	}

	TEST(TEST_CLASS, PacketWithSizeTooSmallIsNotValid) {
		// Assert:
		AssertPacketIsNotValid([](auto& packet) { --packet.Size; });
	}

	TEST(TEST_CLASS, PacketWithSizeTooLargeIsNotValid) {
		// Assert:
		AssertPacketIsNotValid([](auto& packet) { ++packet.Size; });
	}

	TEST(TEST_CLASS, PacketWithCorrectTypeAndSizeIsValid) {
		// Arrange:
		auto pPacket = CreateSharedPacket<Packet>();
		pPacket->Type = Test_Packet_Type;

		// Act:
		auto isValid = IsPacketValid(*pPacket, Test_Packet_Type);

		// Assert:
		EXPECT_TRUE(isValid);
	}

	// endregion
}}
