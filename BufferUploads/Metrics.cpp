// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Metrics.h"
#include "IBufferUploads.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StreamUtils.h"
#include "../Core/Prefix.h"  // for dimof
#include <algorithm>

namespace BufferUploads
{
	CommandListMetrics::CommandListMetrics()
	{
		XlZeroMemory(_bytesUploaded);
		XlZeroMemory(_bytesCreated);
		XlZeroMemory(_stagingBytesUsed);
		XlZeroMemory(_countCreations);
		XlZeroMemory(_countDeviceCreations);
		XlZeroMemory(_countUploaded);
		_bytesUploadTotal = _contextOperations = _deviceCreateOperations = 0;
		_resolveTime = _commitTime = 0;
		_waitTime = _processingStart = _processingEnd = 0;
		_framePriorityStallTime = 0;
		_batchedUploadBytes = _batchedUploadCount = 0;
		_wakeCount = 0;
		_frameId = 0;
		_retirementCount = 0;
	}

	CommandListMetrics::CommandListMetrics(const CommandListMetrics& cloneFrom)
	{
		this->operator=(cloneFrom);
	}

	const CommandListMetrics& CommandListMetrics::operator=(const CommandListMetrics& cloneFrom)
	{
		std::copy(cloneFrom._bytesUploaded, &cloneFrom._bytesUploaded[dimof(cloneFrom._bytesUploaded)], _bytesUploaded);
		std::copy(cloneFrom._bytesCreated, &cloneFrom._bytesCreated[dimof(cloneFrom._bytesCreated)], _bytesCreated);
		std::copy(cloneFrom._stagingBytesUsed, &cloneFrom._stagingBytesUsed[dimof(cloneFrom._stagingBytesUsed)], _stagingBytesUsed);
		_bytesUploadTotal = cloneFrom._bytesUploadTotal;
		std::copy(cloneFrom._countCreations, &cloneFrom._countCreations[dimof(cloneFrom._countCreations)], _countCreations);
		std::copy(cloneFrom._countDeviceCreations, &cloneFrom._countDeviceCreations[dimof(cloneFrom._countDeviceCreations)], _countDeviceCreations);
		std::copy(cloneFrom._countUploaded, &cloneFrom._countUploaded[dimof(cloneFrom._bytesUploaded)], _countUploaded);
		_contextOperations = cloneFrom._contextOperations;
		_deviceCreateOperations = cloneFrom._deviceCreateOperations;
		_assemblyLineMetrics = cloneFrom._assemblyLineMetrics;
		_retirementCount = cloneFrom._retirementCount;
		std::copy(cloneFrom._retirements, &cloneFrom._retirements[std::min(unsigned(dimof(cloneFrom._retirements)), cloneFrom._retirementCount)], _retirements);
		_retirementsOverflow = cloneFrom._retirementsOverflow;
		_resolveTime = cloneFrom._resolveTime;
		_commitTime = cloneFrom._commitTime;
		_waitTime = cloneFrom._waitTime; _processingStart = cloneFrom._processingStart; _processingEnd = cloneFrom._processingEnd;
		_framePriorityStallTime = cloneFrom._framePriorityStallTime;
		_batchedUploadBytes = cloneFrom._batchedUploadBytes; _batchedUploadCount = cloneFrom._batchedUploadCount;
		_wakeCount = cloneFrom._wakeCount; _frameId = cloneFrom._frameId;
		return *this;
	}

	AssemblyLineMetrics::AssemblyLineMetrics()
	{
		_transactionCount = _temporaryTransactionsAllocated = _longTermTransactionsAllocated = _queuedPrepareStaging = _queuedTransferStagingToFinal = _queuedCreateFromDataPacket = 0;
		_peakPrepareStaging = _peakTransferStagingToFinal = _peakCreateFromDataPacket = 0;
		XlZeroMemory(_queuedBytes);
	}

	std::ostream& operator<<(std::ostream& str, const CommandListMetrics& metrics)
	{
		str << " Metric               | Texture              | Vertex               | Index" << std::endl;

		str << " "; str.width(20); str << "Bytes Uploaded";
		str << " | "; str.width(20); str << ByteCount{metrics._bytesUploaded[(unsigned)UploadDataType::Texture]};
		str << " | "; str.width(20); str << ByteCount{metrics._bytesUploaded[(unsigned)UploadDataType::Vertex]};
		str << " | "; str.width(20); str << ByteCount{metrics._bytesUploaded[(unsigned)UploadDataType::Index]};
		str << std::endl;

		str << " "; str.width(20); str << "Bytes Created";
		str << " | "; str.width(20); str << ByteCount{metrics._bytesCreated[(unsigned)UploadDataType::Texture]};
		str << " | "; str.width(20); str << ByteCount{metrics._bytesCreated[(unsigned)UploadDataType::Vertex]};
		str << " | "; str.width(20); str << ByteCount{metrics._bytesCreated[(unsigned)UploadDataType::Index]};
		str << std::endl;

		str << " "; str.width(20); str << "Staging Bytes";
		str << " | "; str.width(20); str << ByteCount{metrics._stagingBytesUsed[(unsigned)UploadDataType::Texture]};
		str << " | "; str.width(20); str << ByteCount{metrics._stagingBytesUsed[(unsigned)UploadDataType::Vertex]};
		str << " | "; str.width(20); str << ByteCount{metrics._stagingBytesUsed[(unsigned)UploadDataType::Index]};
		str << std::endl;

		str << " "; str.width(20); str << "Creations";
		str << " | "; str.width(20); str << metrics._countCreations[(unsigned)UploadDataType::Texture];
		str << " | "; str.width(20); str << metrics._countCreations[(unsigned)UploadDataType::Vertex];
		str << " | "; str.width(20); str << metrics._countCreations[(unsigned)UploadDataType::Index];
		str << std::endl;

		str << " "; str.width(20); str << "Dev Creations";
		str << " | "; str.width(20); str << metrics._countDeviceCreations[(unsigned)UploadDataType::Texture];
		str << " | "; str.width(20); str << metrics._countDeviceCreations[(unsigned)UploadDataType::Vertex];
		str << " | "; str.width(20); str << metrics._countDeviceCreations[(unsigned)UploadDataType::Index];
		str << std::endl;

		str << " "; str.width(20); str << "Uploaded";
		str << " | "; str.width(20); str << metrics._countUploaded[(unsigned)UploadDataType::Texture];
		str << " | "; str.width(20); str << metrics._countUploaded[(unsigned)UploadDataType::Vertex];
		str << " | "; str.width(20); str << metrics._countUploaded[(unsigned)UploadDataType::Index];
		str << std::endl;

		str << "Batched Bytes Uploaded: " << ByteCount{metrics._batchedUploadBytes} << " in " << metrics._batchedUploadCount << " steps " << std::endl;
		str << "Total Bytes Uploaded: " << ByteCount{metrics._bytesUploadTotal} << std::endl;
		str << "Context Operations: " << metrics._contextOperations << std::endl;
		str << "Dev create operations: " << metrics._deviceCreateOperations << std::endl;
		str << "Wake count: " << metrics._wakeCount << std::endl;

		return str;
	}

	std::ostream& operator<<(std::ostream& str, const PoolSystemMetrics& metrics)
	{
		str << "Resource Pools:" << std::endl;
		for (unsigned c=0; c<metrics._resourcePools.size(); ++c)
			str << StreamIndent{4} << "[" << c << "] Current size: " << metrics._resourcePools[c]._currentSize << ", Peak size: " << metrics._resourcePools[c]._peakSize << std::endl;
		str << "Staging Pools:" << std::endl;
		for (unsigned c=0; c<metrics._stagingPools.size(); ++c)
			str << StreamIndent{4} << "[" << c << "] Current size: " << metrics._stagingPools[c]._currentSize << ", Peak size: " << metrics._stagingPools[c]._peakSize << std::endl;
		return str;
	}
}
