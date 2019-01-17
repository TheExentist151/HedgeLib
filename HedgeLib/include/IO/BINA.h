#ifndef HBINA_H_INCLUDED
#define HBINA_H_INCLUDED
#include "nodes.h"
#include "dataSignature.h"
#include "offsets.h"
#include "file.h"
#include "reflect.h"
#include <stdexcept>
#include <cstdint>
#include <cstddef>
#include <cstdio> // TODO?
#include <string_view> // TODO?
#include <vector>
#include <array>
#include <memory>
#include <filesystem>
#include <typeinfo> // TODO?
#include <algorithm>
#include <iostream> // TODO

namespace HedgeLib::IO::BINA
{
	static constexpr DataSignature BINASignature = "BINA";
	static constexpr DataSignature DATASignature = "DATA";

	static constexpr char BigEndianFlag = 'B';
	static constexpr char LittleEndianFlag = 'L';

	enum BINAOffsetTypes : std::uint8_t
	{
		SixBit = 0x40,
		FourteenBit = 0x80,
		ThirtyBit = 0xC0
	};

	struct DBINAV2Header
	{
		DataSignature Signature = BINASignature;
		std::array<char, 3> Version { '2', '1', '0' };
		char EndianFlag = LittleEndianFlag;
		std::uint32_t FileSize = 0;
		std::uint16_t NodeCount = 1;

		constexpr DBINAV2Header() = default;
		constexpr DBINAV2Header(const std::array<char, 3> version,
			const char endianFlag = LittleEndianFlag) noexcept :
			Version(version), EndianFlag(endianFlag) {}

		static constexpr long FileSizeOffset = (sizeof(Signature) +
			sizeof(Version) + sizeof(EndianFlag));
	};

	struct DBINAV2NodeHeader final
	{
	private:
		DataSignature signature;
		std::uint32_t size = 0;

	public:
		DBINAV2NodeHeader() = default;
		constexpr DBINAV2NodeHeader(const DataSignature signature)
			noexcept : signature(signature), size(0) {}

		ENDIAN_SWAP(size);

		static constexpr std::uintptr_t SizeOffset = (sizeof(signature));

		constexpr DataSignature Signature() const noexcept
		{
			return signature;
		}

		constexpr std::uint32_t Size() const noexcept
		{
			return size;
		}
	};

	template<template<typename> class OffsetType>
	std::vector<std::uint32_t> GetOffsets(std::uint8_t* eof,
		std::uint32_t offsetTableLen, std::uintptr_t origin)
	{
		// TODO: Big endian support
		auto offsets = std::vector<std::uint32_t>();
		auto d = reinterpret_cast<OffsetType<std::uint8_t>*>(origin);
		std::uint8_t* o = reinterpret_cast<std::uint8_t*>(
			eof - offsetTableLen);

		constexpr std::uint8_t shiftAmount = // 1 if 64-bit, 0 if 32-bit
			(sizeof(OffsetType<std::uint8_t>) ==
			 sizeof(DataOffset64<std::uint8_t>));

		for (; o < eof; ++o)
		{
			switch (*o & 0xC0)
			{
			case SixBit:
				d += (*o & 0x3F) >> shiftAmount;
				break;

			case FourteenBit:
			{
				std::uint16_t b = static_cast<std::uint16_t>(*o & 0x3F) << 8;
				b |= *(++o);
				b >>= shiftAmount;

				d += b;
				break;
			}

			case ThirtyBit:
			{
				std::uint32_t b = static_cast<std::uint32_t>(*o & 0x3F) << 24;
				b |= *(++o) << 16;
				b |= *(++o) << 8;
				b |= *(++o);
				b >>= shiftAmount;

				d += b;
				break;
			}

			default:
				break;
			}

			offsets.push_back(static_cast<std::uint32_t>(
				reinterpret_cast<std::uintptr_t>(d) - origin));
		}

		return offsets;
	}

	template<template<typename> class OffsetType>
	void FixOffsets(std::uint8_t* eof, std::uint32_t offsetTableLen,
		std::uintptr_t origin, const bool swapEndianness = false)
	{
		// TODO: Big endian support
		auto d = reinterpret_cast<OffsetType<std::uint8_t>*>(origin);
		std::uint8_t* o = reinterpret_cast<std::uint8_t*>(
			eof - offsetTableLen);

		constexpr std::uint8_t shiftAmount = // 1 if 64-bit, 0 if 32-bit
			(sizeof(OffsetType<std::uint8_t>) ==
			 sizeof(DataOffset64<std::uint8_t>));

		for (; o < eof; ++o)
		{
			switch (*o & 0xC0)
			{
			case SixBit:
				d += (*o & 0x3F) >> shiftAmount;
				break;

			case FourteenBit:
			{
				std::uint16_t b = static_cast<std::uint16_t>(*o & 0x3F) << 8;
				b |= *(++o);
				b >>= shiftAmount;

				d += b;
				break;
			}

			case ThirtyBit:
			{
				std::uint32_t b = static_cast<std::uint32_t>(*o & 0x3F) << 24;
				b |= *(++o) << 16;
				b |= *(++o) << 8;
				b |= *(++o);
				b >>= shiftAmount;

				d += b;
				break;
			}

			default:
				return;
			}
			
			d->Fix(origin, swapEndianness);
		}
	}

	struct DBINAV2DataNode
	{
		static constexpr std::size_t PaddingSize = 0x18;
		static constexpr std::uintptr_t SizeOffset = DBINAV2NodeHeader::SizeOffset;

		DBINAV2NodeHeader Header = DATASignature;
		DataOffset32<char> StringTableOffset = nullptr;
		std::uint32_t StringTableSize = 0;
		std::uint32_t OffsetTableSize = 0;
		std::uint16_t RelativeDataOffset = PaddingSize; // ?
		std::array<std::uint8_t, PaddingSize> Padding {};

		ENDIAN_SWAP(StringTableSize,
			OffsetTableSize, RelativeDataOffset);

		constexpr std::uint32_t Size() const noexcept
		{
			return Header.Size();
		}

		template<template<typename> class OffsetType>
		void FixOffsets(const bool swapEndianness = false)
		{
			if (swapEndianness)
				EndianSwap();

			std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(this);
			if (StringTableSize)
			{
				StringTableOffset.Fix(ptr + sizeof(DBINAV2DataNode), swapEndianness);
			}

			if (OffsetTableSize)
			{
				BINA::FixOffsets<OffsetType>(reinterpret_cast
					<std::uint8_t*>(ptr + Header.Size()),
					OffsetTableSize, ptr + sizeof(DBINAV2DataNode), swapEndianness);
			}
		}
	};

	template<template<typename> class OffsetType>
	struct DBINAString
	{
		OffsetType<char> StringOffset = nullptr;
		OffsetType<char> UnknownOffset = nullptr;

		constexpr DBINAString() = default;
		DBINAString(const char* c) : StringOffset(c) {}

		ENDIAN_SWAP(StringOffset, UnknownOffset);
		OFFSETS(StringOffset, UnknownOffset);

		operator char*() const noexcept
		{
			return StringOffset;
		}
	};

	using BINAString32 = DBINAString<DataOffset32>;
	using BINAString64 = DBINAString<DataOffset64>;

	void WriteOffsetsSorted(const HedgeLib::IO::File& file,
		std::vector<std::uint32_t>& offsets) noexcept;

	inline void WriteOffsets(const HedgeLib::IO::File& file,
		std::vector<std::uint32_t>& offsets) noexcept
	{
		std::sort(offsets.begin(), offsets.end());
		WriteOffsetsSorted(file, offsets);
	}

	// TODO: Finish the ReadNodes functions somehow

	//template<template<typename> class OffsetType>
	//std::vector<NodePointer<BINAV2Node>> ReadNodesV2(
	//	const HedgeLib::IO::File& file, const BINAV2Header& header)
	//{
	//	std::vector<NodePointer<BINAV2Node>> nodes(static_cast
	//		<const std::size_t>(header.NodeCount));

	//	for (std::uint16_t i = 0; i < header.NodeCount; ++i)
	//	{
	//		// Read node
	//		nodes[i] = ReadNode<BINAV2NodeHeader>(file);

	//		// Fix offsets if DATA Node
	//		if (nodes[i]->Header.Signature() == DATASignature)
	//		{
	//			reinterpret_cast<BINAV2DataNode*>(nodes[i].get())->
	//				FixOffsets<OffsetType>();
	//		}
	//	}

	//	return nodes;
	//}

	template<class DataType, template<typename> class OffsetType>
	NodePointer<DataType> ReadV2(const HedgeLib::IO::File& file,
		const DBINAV2Header& header)
	{
		// Find DATA Node, read it, then return a smart pointer to it
		DBINAV2NodeHeader nodeHeader = {};
		for (std::uint16_t i = 0; i < header.NodeCount; ++i)
		{
			if (!file.Read(&nodeHeader, sizeof(nodeHeader), 1))
				throw std::runtime_error("Could not read BINA node header!");

			if (nodeHeader.Signature() == DATASignature)
			{
				// Swap endianness of header if necessary
				if (header.EndianFlag == BigEndianFlag)
					nodeHeader.EndianSwap();

				// Read node and fix offsets
				NodePointer<DataType> data = ReadNode<DBINAV2NodeHeader,
					DataType>(file, nodeHeader);

				// gcc errors unless we include ".template"
				data->Header.template FixOffsets<OffsetType>(
					header.EndianFlag == BigEndianFlag);

				// Swap endianness of non-offsets if necessary
				if (header.EndianFlag == BigEndianFlag)
					data->EndianSwap();

				return data;
			}
			else
			{
				// This isn't the DATA node; skip it!
				file.Seek(nodeHeader.Size() -
					sizeof(nodeHeader), SEEK_CUR);
			}
		}

		throw std::runtime_error("Could not find BINA DATA node!");
	}

	//template<template<typename> class OffsetType>
	//std::vector<NodePointer<BINAV2Node>> ReadNodes(
	//	const HedgeLib::IO::File& file)
	//{
	//	// TODO: Autodetect BINA header type

	//	auto header = BINAV2Header();
	//	if (!file.Read(&header, sizeof(header), 1))
	//		throw std::runtime_error("Could not read BINA header!");

	//	return ReadNodesV2<OffsetType>(file, header);
	//}

	template<class DataType, template<typename> class OffsetType>
	NodePointer<DataType> Read(const HedgeLib::IO::File& file)
	{
		// TODO: Autodetect BINA header type

		auto header = DBINAV2Header();
		if (!file.Read(&header, sizeof(header), 1))
			throw std::runtime_error("Could not read BINA header!");

		return ReadV2<DataType, OffsetType>(file, header);
	}

	/*template<template<typename> class OffsetType>
	std::vector<NodePointer<BINAV2Node>> LoadNodes(
		const std::filesystem::path filePath)
	{
		HedgeLib::IO::File file = HedgeLib::IO::File::OpenRead(filePath);
		return ReadNodes<OffsetType>(file);
	}*/

	template<class DataType, template<typename> class OffsetType>
	NodePointer<DataType> Load(const std::filesystem::path filePath)
	{
		HedgeLib::IO::File file = HedgeLib::IO::File::OpenRead(filePath);
		return Read<DataType, OffsetType>(file);
	}

	template<class DataType, template<typename> class OffsetType>
	void WriteV2(const HedgeLib::IO::File& file,
		const DataType& data, const DBINAV2Header& header)
	{
		// Write header
		long startPos = file.Tell(); // So we can safely append to files as well
		file.Write(&header, sizeof(header), 1);

		// Write data node and its children
		std::vector<std::uint32_t> offsets;
		const long origin = (file.Tell() + sizeof(data.Header));
		data.WriteRecursive(file, origin, &offsets);

		// TODO: Write arrays properly
		// TODO: Write strings properly

		// Write the offset table
		std::uint32_t offTablePos = static_cast<std::uint32_t>(file.Tell());
		WriteOffsets(file, offsets);
		long endPos = file.Tell();

		// Fix header file size
		std::uint32_t size = static_cast<std::uint32_t>(endPos - startPos);
		file.Seek(startPos + header.FileSizeOffset);
		file.Write(&size, sizeof(size), 1);

		// Fix Node Size
		std::uint32_t nodeSize = static_cast<std::uint32_t>(
			endPos - sizeof(BINA::DBINAV2Header));

		file.Seek(startPos + (sizeof(BINA::DBINAV2Header) +
			BINA::DBINAV2DataNode::SizeOffset));

		file.Write(&nodeSize, sizeof(nodeSize), 1);

		// Fix String Table Offset
		// TODO
		/*stringTablePos -= 0x40;
		file.Seek(startPos + sizeof(BINA::DBINAV2Header) + 8);
		file.Write(&stringTablePos, sizeof(stringTablePos), 1);*/

		// Fix String Table Size
		// TODO
		offTablePos -= 0x40;
		/*stringTablePos = (offTablePos - stringTablePos);

		file.Seek(startPos + sizeof(BINA::DBINAV2Header) + 12);
		file.Write(&stringTablePos, sizeof(std::uint32_t), 1);*/

		// Fix Offset Table Size
		offTablePos = static_cast<std::uint32_t>(
			nodeSize - offTablePos - sizeof(BINA::DBINAV2DataNode));

		file.Seek(startPos + sizeof(BINA::DBINAV2Header) + 16);
		file.Write(&offTablePos, sizeof(std::uint32_t), 1);

		// Go to end so future write operations continue on afterwards if needed
		file.Seek(endPos);
	}

	template<class DataType, template<typename> class OffsetType>
	void WriteV2(const HedgeLib::IO::File& file,
		const DataType& data)
	{
		DBINAV2Header header = DBINAV2Header();
		WriteV2<DataType, OffsetType>(file, data, header);
	}

	template<class DataType, template<typename> class OffsetType>
	void SaveV2(const std::filesystem::path filePath,
		const DataType& data, const DBINAV2Header& header)
	{
		HedgeLib::IO::File file = HedgeLib::IO::File::OpenWrite(filePath);
		WriteV2<DataType, OffsetType>(file, data, header);
	}

	template<class DataType, template<typename> class OffsetType>
	void SaveV2(const std::filesystem::path filePath,
		const DataType& data)
	{
		HedgeLib::IO::File file = HedgeLib::IO::File::OpenWrite(filePath);
		WriteV2<DataType, OffsetType>(file, data);
	}
}
#endif