Here is a complete, lightweight C example demonstrating the **callback pattern**: defining the function signature, implementing the registration mechanism, and invoking the callback.

```c
#include <stdio.h>

// 1. Define the callback function signature (takes an int, returns void)
typedef void (*DataCallback)(int data);

// Global or module-scoped pointer to store the registered callback
static DataCallback g_callback = NULL;

// 2. Register Function: stores the function pointer provided by the caller
void register_data_callback(DataCallback cb) {
    g_callback = cb;
}

// Simulated event driver (e.g., hardware interrupt, timer, async response)
void trigger_event(int value) {
    if (g_callback != NULL) {
        g_callback(value); // Execute registered callback
    } else {
        printf("No callback registered!\n");
    }
}

// 3. Callback Function Implementation
void my_event_handler(int value) {
    printf("[Callback Triggered] Received value: %d\n", value);
}

int main(void) {
    // Register the callback function
    register_data_callback(my_event_handler);

    // Simulate event firing
    trigger_event(42);

    return 0;
}

```

### Key Components

* **Function Pointer Type (`typedef`):** Defines the exact parameter list and return type expected from subscribers.
* **Registration Function:** Saves the function address passed from external code into a local or global function pointer.
* **Callback Execution:** Verifies the pointer is non-`NULL` before calling it with event payload data.
