# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/heap/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/heap -lheap -Wl,--wrap=calloc -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=heap_caps_malloc -Wl,--wrap=heap_caps_free -Wl,--wrap=heap_caps_realloc -Wl,--wrap=heap_caps_malloc_default -Wl,--wrap=heap_caps_realloc_default
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += heap
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/heap/linker.lf
component-heap-build: 
