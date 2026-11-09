#include "NIDX8Renderer.h"

#include "ExceptionUtil.h"

namespace NI {
	DX8AdapterDesc* DX8Renderer::getCurrentAdapter() const {
		if (systemDesc == nullptr) {
			return nullptr;
		}
		return systemDesc->adapters[currentAdapterIndex];
	}

	//
	// Runs the engine's destructor after the skin partition buffers are released without reading their keys.
	//

	void DX8Renderer::destruct() {
#if defined(SE_NI_DX8RENDERER_FNADDR_DTOR) && SE_NI_DX8RENDERER_FNADDR_DTOR > 0
		const auto NI_DX8Renderer_dtor = reinterpret_cast<void(__thiscall*)(DX8Renderer*)>(SE_NI_DX8RENDERER_FNADDR_DTOR);
		releaseSkinPartitionBufferData();
		NI_DX8Renderer_dtor(this);
#else
		throw not_implemented_exception();
#endif
	}

	void DX8Renderer::releaseSkinPartitionBufferData() {
#if defined(SE_MEMORY_FNADDR_DELETE) && SE_MEMORY_FNADDR_DELETE > 0
		for (size_t bucket = 0; bucket < skinPartitionBufferData.bucketCount; ++bucket) {
			while (const auto node = skinPartitionBufferData.buckets[bucket]) {
				skinPartitionBufferData.buckets[bucket] = node->nextNode;

				if (const auto bufferData = node->value) {
					if (bufferData->d3dVertexBuffer) {
						bufferData->d3dVertexBuffer->Release();
					}
					if (bufferData->indexBuffer) {
						bufferData->indexBuffer->Release();
					}
					se::memory::_delete(bufferData);
				}

				skinPartitionBufferData.deleteKeyValuePair(&node->keyValuePair);
				se::memory::_delete(node);
			}
		}
		skinPartitionBufferData.count = 0;
#else
		throw not_implemented_exception();
#endif
	}
}
