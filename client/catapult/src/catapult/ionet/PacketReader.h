#pragma once
#include "Packet.h"
#include "catapult/utils/Logging.h"

namespace catapult { namespace ionet {

	/// Stateful packet reader.
	/// \note Behavior is undefined if error has been encountered on any previous call.
	class PacketReader {
	public:
		/// Creates a reader around \a packet.
		explicit PacketReader(const Packet& packet)
				: m_pData(packet.Data())
				, m_numRemainingBytes(packet.Size)
				, m_hasError(false) {
			require(sizeof(PacketHeader), "constructor");
			m_numRemainingBytes -= sizeof(PacketHeader);
		}

	public:
		/// Returns \c true if the reader has consumed all data, \c false otherwise.
		bool empty() const {
			return 0 == m_numRemainingBytes || m_hasError;
		}

		/// Returns \c true if a reading error has been encountered, \c false otherwise.
		bool hasError() const {
			return m_hasError;
		}

	public:
		/// Reads a fixed-sized value from the packet.
		template<typename TValue>
		const TValue* readFixed() {
			require(sizeof(TValue), "readFixed");
			if (hasError())
				return nullptr;

			const auto& value = reinterpret_cast<const TValue&>(*m_pData);
			advance(sizeof(TValue));
			return &value;
		}

		/// Reads a variable-sized value from the packet.
		template<typename TEntity>
		const TEntity* readVariable() {
			auto pSize = readFixed<uint32_t>();
			if (!pSize)
				return nullptr;

			// readFixed above (for size) advances the data pointer past the size, but the variable entity should point to the size
			rewind(sizeof(uint32_t));
			require(*pSize, "readVariable");
			if (hasError())
				return nullptr;

			const auto& entity = reinterpret_cast<const TEntity&>(*m_pData);
			advance(*pSize);
			return &entity;
		}

	private:
		void require(uint32_t numBytes, const char* message) {
			if (m_numRemainingBytes >= numBytes)
				return;

			CATAPULT_LOG(warning) << message << ": requested (" << numBytes << ") bytes with only " << m_numRemainingBytes << " remaining";
			m_hasError = true;
		}

		void advance(uint32_t numBytes) {
			m_pData += numBytes;
			m_numRemainingBytes -= numBytes;
		}

		void rewind(uint32_t numBytes) {
			m_pData -= numBytes;
			m_numRemainingBytes += numBytes;
		}

	private:
		const uint8_t* m_pData;
		uint32_t m_numRemainingBytes;
		bool m_hasError;
	};
}}
