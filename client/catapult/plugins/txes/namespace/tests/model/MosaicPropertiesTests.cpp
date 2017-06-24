#include "src/model/MosaicProperties.h"
#include "catapult/utils/Casting.h"
#include "tests/test/MosaicTestUtils.h"
#include "tests/test/nodeps/Equality.h"
#include "tests/TestHarness.h"

namespace catapult { namespace model {

#define TEST_CLASS MosaicPropertiesTests

	namespace {
		void SetTestPropertyValues(MosaicProperties::PropertyValuesContainer& values) {
			// Arrange:
			values[0] = utils::to_underlying_type(MosaicFlags::Supply_Mutable | MosaicFlags::Levy_Mutable);
			values[1] = 5u;
			values[2] = 234u;
		}

		MosaicProperties CreateProperties(
				model::MosaicFlags flags,
				uint8_t divisibility,
				std::initializer_list<model::MosaicProperty> optionalProperties) {
			MosaicProperties::PropertyValuesContainer values;
			for (auto i = 0u; i < values.size(); ++i)
				values[i] = 0xDEADBEAF;

			values[0] = utils::to_underlying_type(flags);
			values[1] = divisibility;

			for (const auto& property : optionalProperties)
				values[utils::to_underlying_type(property.Id)] = property.Value;

			return MosaicProperties::FromValues(values);
		}
	}

	// region ctor

	TEST(TEST_CLASS, CanCreateMosaicProperties) {
		// Arrange:
		MosaicProperties::PropertyValuesContainer values;
		SetTestPropertyValues(values);

		// Act:
		auto properties = MosaicProperties::FromValues(values);

		// Assert:
		EXPECT_EQ(3u, properties.size());

		EXPECT_TRUE(properties.is(MosaicFlags::Supply_Mutable));
		EXPECT_FALSE(properties.is(MosaicFlags::Transferable));
		EXPECT_TRUE(properties.is(MosaicFlags::Levy_Mutable));

		EXPECT_EQ(5u, properties.divisibility());
		EXPECT_EQ(ArtifactDuration(234u), properties.duration());
	}

	// endregion

	// region extract properties

	TEST(TEST_CLASS, ExtractPropertiesCanExtractRequiredProperties) {
		// Arrange:
		auto flags = MosaicFlags::Supply_Mutable | MosaicFlags::Transferable | MosaicFlags::Levy_Mutable;
		auto header = MosaicPropertiesHeader{ 0, flags, 123 };

		// Act:
		auto properties = ExtractAllProperties(header, nullptr);

		// Assert:
		auto expected = CreateProperties(flags, 123, { { MosaicPropertyId::Duration, 0 } });
		test::AssertMosaicDefinitionProperties(expected, properties);
	}

	TEST(TEST_CLASS, ExtractPropertiesCanExtractOptionalProperties) {
		// Arrange:
		auto flags = MosaicFlags::Supply_Mutable | MosaicFlags::Transferable | MosaicFlags::Levy_Mutable;
		auto optionalProperties = std::vector<MosaicProperty>{ { MosaicPropertyId::Duration, 12345678u } };
		auto header = MosaicPropertiesHeader{ static_cast<uint8_t>(optionalProperties.size()), flags, 123 };

		// Act:
		auto properties = ExtractAllProperties(header, optionalProperties.data());

		// Assert:
		auto expected = CreateProperties(flags, 123, { { MosaicPropertyId::Duration, 12345678u } });
		test::AssertMosaicDefinitionProperties(expected, properties);
	}

	TEST(TEST_CLASS, ExtractPropertiesIgnoresOutOfRangeProperties) {
		// Arrange:
		auto flags = MosaicFlags::Supply_Mutable | MosaicFlags::Transferable | MosaicFlags::Levy_Mutable;
		auto optionalProperties = std::vector<MosaicProperty>{
			{ static_cast<MosaicPropertyId>(0), 0xDEAD }, // reserved (required)
			{ MosaicPropertyId::Duration, 12345678u }, // valid
			{ static_cast<MosaicPropertyId>(3), 0xDEAD }, // id too large
			{ static_cast<MosaicPropertyId>(0xFF), 0xDEAD } // id too large
		};
		auto header = MosaicPropertiesHeader{ static_cast<uint8_t>(optionalProperties.size()), flags, 123 };

		// Act:
		auto properties = ExtractAllProperties(header, optionalProperties.data());

		// Assert:
		auto expected = CreateProperties(flags, 123, { { MosaicPropertyId::Duration, 12345678u } });
		test::AssertMosaicDefinitionProperties(expected, properties);
	}

	// endregion

	// region iteration

	TEST(TEST_CLASS, CanIterateOverAllProperties) {
		// Arrange:
		MosaicProperties::PropertyValuesContainer seedValues;
		SetTestPropertyValues(seedValues);
		auto properties = MosaicProperties::FromValues(seedValues);

		// Act:
		std::array<MosaicProperty, Num_Mosaic_Properties> extractedProperties;
		std::vector<uint8_t> ids;
		for (auto iter = properties.cbegin(); properties.cend() != iter; ++iter) {
			auto id = utils::to_underlying_type(iter->Id);
			ids.push_back(id);
			extractedProperties[id] = *iter;
		}

		// Assert:
		EXPECT_EQ(std::vector<uint8_t> ({ 0, 1, 2 }), ids);
		for (auto i = 0u; i < seedValues.size(); ++i) {
			EXPECT_EQ(i, utils::to_underlying_type(extractedProperties[i].Id)) << "property at " << i;
			EXPECT_EQ(seedValues[i], extractedProperties[i].Value) << "property at " << i;
		}
	}

	// endregion

	// region equality operators

	namespace {
		std::unordered_set<std::string> GetEqualTags() {
			return { "default", "copy" };
		}

		std::unordered_map<std::string, MosaicProperties> GenerateEqualityInstanceMap() {
			return {
				{ "default", MosaicProperties::FromValues({ { 2, 7, 5 } }) },
				{ "copy", MosaicProperties::FromValues({ { 2, 7, 5 } }) },

				{ "diff[0]", MosaicProperties::FromValues({ { 1, 7, 5 } }) },
				{ "diff[1]", MosaicProperties::FromValues({ { 2, 9, 5 } }) },
				{ "diff[2]", MosaicProperties::FromValues({ { 2, 7, 6 } }) },
				{ "reverse", MosaicProperties::FromValues({ { 5, 7, 2 } }) },
				{ "diff-all", MosaicProperties::FromValues({ { 1, 8, 6 } }) }
			};
		}
	}

	TEST(TEST_CLASS, OperatorEqualReturnsTrueOnlyForEqualValues) {
		// Assert:
		test::AssertOperatorEqualReturnsTrueForEqualObjects("default", GenerateEqualityInstanceMap(), GetEqualTags());
	}

	TEST(TEST_CLASS, OperatorNotEqualReturnsTrueOnlyForUnequalValues) {
		// Assert:
		test::AssertOperatorNotEqualReturnsTrueForUnequalObjects("default", GenerateEqualityInstanceMap(), GetEqualTags());
	}

	// endregion
}}
