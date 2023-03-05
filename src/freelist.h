#pragma once

#define FREE_LIST_INSERT(list, element) ({\
	int32_t index; \
	if (list.firstFree == -1) { \
		index = list.elementCount++; \
	} else { \
		index = list.firstFree; \
		list.firstFree = list.data[index].next; \
	} \
	list.data[index] = element; \
	list.data[index].next = list.head; \
	list.head = index; \
	index; \
})
