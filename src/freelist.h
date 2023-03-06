#pragma once
#define FREE_LIST_INSERT(list, element) ({ \
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
#define FREE_LIST_ITERATE(list, code) ({ \
	int32_t idx = list.head; \
	while (idx != -1) { \
		__auto_type* element = &list.data[idx]; \
		code \
		idx = element->next; \
	} \
})
#define FREE_LIST_ITERATE_REMOVABLE(list, code) ({ \
	int32_t* idx = &list.head; \
	while (*idx != -1) { \
		__auto_type* element = &list.data[*idx]; \
		code \
		idx = &element->next; \
	} \
})
#define FREE_LIST_REMOVE(list) ({ \
	int32_t temp = *idx; \
	*idx = element->next; \
	element->next = list.firstFree; \
	list.firstFree = temp; \
	continue; \
})