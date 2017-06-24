#include "catapult/api/RemoteChainApi.h"
#include "catapult/api/ChainPackets.h"
#include "catapult/model/TransactionPlugin.h"
#include "tests/test/other/RemoteApiTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace api {

	namespace {
		std::shared_ptr<ionet::Packet> CreatePacketWithBlocks(uint32_t numBlocks, Height startHeight) {
			uint32_t payloadSize = numBlocks * sizeof(model::Block);
			auto pPacket = ionet::CreateSharedPacket<ionet::Packet>(payloadSize);
			test::FillWithRandomData({ pPacket->Data(), payloadSize });

			auto pData = pPacket->Data();
			for (auto i = 0u; i < numBlocks; ++i, pData += sizeof(model::Block)) {
				auto& block = reinterpret_cast<model::Block&>(*pData);
				block.Size = sizeof(model::Block);
				block.Type = model::EntityType::Block;
				block.Height = startHeight + Height(i);
			}

			return pPacket;
		}

		struct ChainInfoTraits {
			static auto Invoke(const ChainApi& api) {
				return api.chainInfo();
			}

			static auto CreateValidResponsePacket() {
				auto pResponsePacket = ionet::CreateSharedPacket<ChainInfoResponse>();
				pResponsePacket->Height = Height(625);
				pResponsePacket->ScoreHigh = 0x1234567812345678;
				pResponsePacket->ScoreLow = 0xABCDABCDABCDABCD;
				return pResponsePacket;
			}

			static auto CreateMalformedResponsePacket() {
				// just change the size because no responses are intrinsically invalid
				auto pResponsePacket = CreateValidResponsePacket();
				--pResponsePacket->Size;
				return pResponsePacket;
			}

			static void ValidateRequest(const ionet::Packet& packet) {
				EXPECT_TRUE(ionet::IsPacketValid(packet, ChainInfoResponse::Packet_Type));
			}

			static void ValidateResponse(const ionet::Packet&, const ChainInfo& info) {
				EXPECT_EQ(Height(625), info.Height);

				auto scoreArray = info.Score.toArray();
				EXPECT_EQ(0x1234567812345678, scoreArray[0]);
				EXPECT_EQ(0xABCDABCDABCDABCD, scoreArray[1]);
			}
		};

		struct HashesFromTraits {
			static constexpr Height RequestHeight() { return Height(521); }

			static auto Invoke(const ChainApi& api) {
				return api.hashesFrom(RequestHeight());
			}

			static auto CreateValidResponsePacket(uint32_t payloadSize = 3u * sizeof(Hash256)) {
				auto pResponsePacket = ionet::CreateSharedPacket<ionet::Packet>(payloadSize);
				pResponsePacket->Type = ionet::PacketType::Block_Hashes;
				test::FillWithRandomData({ pResponsePacket->Data(), payloadSize });
				return pResponsePacket;
			}

			static auto CreateMalformedResponsePacket() {
				// the packet is malformed because it contains a partial packet (1.5 packets in all)
				return CreateValidResponsePacket(3 * sizeof(Hash256) / 2);
			}

			static void ValidateRequest(const ionet::Packet& packet) {
				auto pRequest = ionet::CoercePacket<BlockHashesRequest>(&packet);
				ASSERT_TRUE(!!pRequest);
				EXPECT_EQ(RequestHeight(), pRequest->Height);
			}

			static void ValidateResponse(const ionet::Packet& response, const model::HashRange& hashes) {
				ASSERT_EQ(3u, hashes.size());

				auto iter = hashes.cbegin();
				for (auto i = 0u; i < hashes.size(); ++i) {
					auto pExpectedHash = response.Data() + i * sizeof(Hash256);
					auto pActualHash = iter->data();
					EXPECT_TRUE(0 == std::memcmp(pExpectedHash, pActualHash, sizeof(Hash256)))
							<< "comparing hashes at " << i;
					++iter;
				}
			}
		};

		struct BlockLastInvoker {
			static constexpr Height RequestHeight() { return Height(0); }

			static auto Invoke(const RemoteChainApi& api) {
				return api.blockLast();
			}
		};

		struct BlockAtInvoker {
			static constexpr Height RequestHeight() { return Height(728); }

			static auto Invoke(const RemoteChainApi& api) {
				return api.blockAt(RequestHeight());
			}
		};

		template<typename TInvoker>
		struct BlockAtTraitsT : public TInvoker {
			static auto CreateValidResponsePacket(uint32_t numBlocks = 1) {
				auto pResponsePacket = CreatePacketWithBlocks(numBlocks, TInvoker::RequestHeight());
				pResponsePacket->Type = ionet::PacketType::Pull_Block;
				return pResponsePacket;
			}

			static auto CreateMalformedResponsePacket() {
				// block-at api can only return a single block
				return CreateValidResponsePacket(2);
			}

			static void ValidateRequest(const ionet::Packet& packet) {
				auto pRequest = ionet::CoercePacket<PullBlockRequest>(&packet);
				ASSERT_TRUE(!!pRequest);
				EXPECT_EQ(TInvoker::RequestHeight(), pRequest->Height);
			}

			static void ValidateResponse(const ionet::Packet& response, const std::shared_ptr<const model::Block>& pBlock) {
				ASSERT_EQ(response.Size - sizeof(ionet::Packet), pBlock->Size);
				ASSERT_EQ(sizeof(model::Block), pBlock->Size);
				EXPECT_EQ(TInvoker::RequestHeight(), pBlock->Height);
				EXPECT_TRUE(0 == std::memcmp(response.Data(), pBlock.get(), pBlock->Size));
			}
		};

		using BlockLastTraits = BlockAtTraitsT<BlockLastInvoker>;
		using BlockAtTraits = BlockAtTraitsT<BlockAtInvoker>;

		struct BlocksFromTraits {
			static constexpr Height RequestHeight() { return Height(823); }

			static auto Invoke(const RemoteChainApi& api) {
				return api.blocksFrom(RequestHeight(), { 200, 1024 });
			}

			static auto CreateValidResponsePacket() {
				auto pResponsePacket = CreatePacketWithBlocks(3, RequestHeight());
				pResponsePacket->Type = ionet::PacketType::Pull_Blocks;
				return pResponsePacket;
			}

			static auto CreateMalformedResponsePacket() {
				// the packet is malformed because it contains a partial block
				auto pResponsePacket = CreateValidResponsePacket();
				--pResponsePacket->Size;
				return pResponsePacket;
			}

			static void ValidateRequest(const ionet::Packet& packet) {
				auto pRequest = ionet::CoercePacket<PullBlocksRequest>(&packet);
				ASSERT_TRUE(!!pRequest);
				EXPECT_EQ(RequestHeight(), pRequest->Height);
				EXPECT_EQ(200u, pRequest->NumBlocks);
				EXPECT_EQ(1024u, pRequest->NumResponseBytes);
			}

			static void ValidateResponse(const ionet::Packet& response, const model::BlockRange& blocks) {
				ASSERT_EQ(3u, blocks.size());

				auto pData = response.Data();
				auto iter = blocks.cbegin();
				for (auto i = 0u; i < blocks.size(); ++i) {
					std::string message = "comparing blocks at " + std::to_string(i);
					const auto& expectedBlock = reinterpret_cast<const model::Block&>(*pData);
					const auto& actualBlock = *iter;
					ASSERT_EQ(expectedBlock.Size, actualBlock.Size) << message;
					EXPECT_EQ(RequestHeight() + Height(i), actualBlock.Height) << message;
					EXPECT_EQ(expectedBlock, actualBlock) << message;
					++iter;
					pData += expectedBlock.Size;
				}
			}
		};

		struct RemoteChainApiBlocklessTraits {
			static auto Create(const std::shared_ptr<ionet::PacketIo>& pPacketIo) {
				return CreateRemoteChainApi(pPacketIo);
			}
		};

		struct RemoteChainApiTraits {
			static auto Create(const std::shared_ptr<ionet::PacketIo>& pPacketIo) {
				return CreateRemoteChainApi(pPacketIo, std::make_shared<model::TransactionRegistry>());
			}
		};
	}

	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApiBlockless, ChainInfo)
	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApiBlockless, HashesFrom)

	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApi, ChainInfo)
	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApi, HashesFrom)
	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApi, BlockLast)
	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_INVALID(RemoteChainApi, BlockAt)
	DEFINE_REMOTE_API_TESTS_EMPTY_RESPONSE_VALID(RemoteChainApi, BlocksFrom)
}}
