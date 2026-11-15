#include "NISortAdjustNode.h"

#include "ExceptionUtil.h"

namespace NI {
	void SortAdjustNode::ctor() {
#if defined(SE_NI_SORTADJUSTNODE_VTBL) && SE_NI_SORTADJUSTNODE_VTBL > 0
		Node::ctor();
		vTable.asNode = reinterpret_cast<Node_vTable*>(SE_NI_SORTADJUSTNODE_VTBL);
		sortingMode = SortAdjustMode::SORTING_INHERIT;
		std::construct_at(&accumulator);
#else
		throw not_implemented_exception();
#endif
	}

	Pointer<SortAdjustNode> SortAdjustNode::create() {
		return construct<SortAdjustNode>();
	}
}
