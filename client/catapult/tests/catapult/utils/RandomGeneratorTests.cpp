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

#include "catapult/utils/RandomGenerator.h"
#include "tests/TestHarness.h"

namespace catapult { namespace utils {

#define TEST_CLASS RandomGeneratorTests

	// region properties

	namespace {
		template<typename TGenerator>
		void AssertRandomGeneratorProperties() {
			// Act + Assert:
			EXPECT_EQ(0u, TGenerator::min());
			EXPECT_EQ(std::numeric_limits<uint64_t>::max(), TGenerator::max());
		}
	}

	TEST(TEST_CLASS, HighEntropyRandomGeneratorExposesCorrectProperties) {
		AssertRandomGeneratorProperties<HighEntropyRandomGenerator>();
	}

	TEST(TEST_CLASS, LowEntropyRandomGeneratorExposesCorrectProperties) {
		AssertRandomGeneratorProperties<LowEntropyRandomGenerator>();
	}

	// endregion

	// region randomness

	namespace {
		constexpr auto Num_Samples = 10'000u;
		constexpr auto Num_Buckets = 16u; // evenly size buckets

		double CalculateChiSquare(const std::array<uint64_t, Num_Buckets>& buckets, uint64_t expectedValue) {
			auto chiSquare = 0.0;
			auto minValue = expectedValue;
			auto maxValue = expectedValue;
			for (auto observedValue : buckets) {
				minValue = std::min(observedValue, minValue);
				maxValue = std::max(observedValue, maxValue);

				auto difference = observedValue - expectedValue;
				chiSquare += static_cast<double>(difference * difference) / static_cast<double>(expectedValue);
			}

			CATAPULT_LOG(debug) << "chiSquare = " << chiSquare << ", min = " << minValue << ", max = " << maxValue;
			return chiSquare;
		}

		double LookupProbability(double chiSquare) {
			auto chiSquareTable = std::initializer_list<std::pair<double, double>>{
				// df = 15
				{ 37.697, 1 - 0.001 },
				{ 35.628, 1 - 0.002 },
				{ 32.801, 1 - 0.005 },
				{ 30.578, 1 - 0.010 },
				{ 27.488, 1 - 0.025 },
				{ 24.996, 1 - 0.050 },
				{ 22.307, 1 - 0.100 },
				{ 18.245, 1 - 0.250 },
				{ 14.339, 1 - 0.500 },
				{ 11.037, 1 - 0.750 },
				{ 8.5470, 1 - 0.900 },
				{ 7.2610, 1 - 0.950 },
				{ 6.2620, 1 - 0.975 },
				{ 5.2290, 1 - 0.990 },
				{ 4.6010, 1 - 0.995 }
			};

			for (const auto& pair : chiSquareTable) {
				if (chiSquare > pair.first) {
					auto probability = pair.second * 100;
					CATAPULT_LOG(debug) << "randomness hypothesis can be rejected with at least " << probability << " percent certainty";
					return probability;
				}
			}

			return 100;
		}

		template<typename TValue>
		size_t GetBucketIndex(TValue value) {
			return value / (std::numeric_limits<TValue>::max() / Num_Buckets + 1);
		}

		template<typename TGenerator>
		void AssertExhibitsRandomness() {
			// Assert: non-deterministic because testing randomness
			test::RunNonDeterministicTest("AssertExhibitsRandomness", []() {
				// Arrange:
				TGenerator generator;

				// Act:
				std::array<uint64_t, Num_Buckets> buckets{};
				for (auto i = 0u; i < Num_Samples; ++i) {
					auto value = generator();
					++buckets[GetBucketIndex(value)];
				}

				// Assert:
				auto chiSquare = CalculateChiSquare(buckets, Num_Samples / Num_Buckets);
				auto probability = LookupProbability(chiSquare);
				return probability < 75.0;
			});
		}

		template<typename TGenerator>
		void AssertFillExhibitsRandomness() {
			// Assert: non-deterministic because testing randomness
			test::RunNonDeterministicTest("AssertFillExhibitsRandomness", []() {
				// Arrange:
				TGenerator generator;

				// Act:
				std::array<uint64_t, Num_Buckets> buckets{};
				for (auto i = 0u; i < Num_Samples / 20; ++i) {
					std::array<uint8_t, 20> values;
					generator.fill(values.data(), values.size());

					for (auto value : values)
						++buckets[GetBucketIndex(value)];
				}

				// Assert:
				auto chiSquare = CalculateChiSquare(buckets, Num_Samples / Num_Buckets);
				auto probability = LookupProbability(chiSquare);
				return probability < 75.0;
			});
		}

		class HighEntropyRandomGeneratorCustomToken : public HighEntropyRandomGenerator {
		public:
			HighEntropyRandomGeneratorCustomToken() : HighEntropyRandomGenerator("/dev/urandom")
			{}
		};
	}

#define DEFINE_RANDOMNESS_TESTS(NAME) \
	TEST(TEST_CLASS, NAME##ExhibitsRandomness) { \
		AssertExhibitsRandomness<NAME>(); \
	} \
	TEST(TEST_CLASS, NAME##FillExhibitsRandomness) { \
		AssertFillExhibitsRandomness<NAME>(); \
	}

	DEFINE_RANDOMNESS_TESTS(HighEntropyRandomGenerator)
	DEFINE_RANDOMNESS_TESTS(HighEntropyRandomGeneratorCustomToken)
	DEFINE_RANDOMNESS_TESTS(LowEntropyRandomGenerator)

	// endregion
}}
