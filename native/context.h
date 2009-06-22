#if defined(__i386)
struct Context
{
    long ebx, esp, ebp, esi, edi, eip;
};
#elif defined(__x86_64)
struct Context
{
    long rbx, rsp, rbp, r12, r13, r14, r15, rip;
};
#else
#error "unsupported architecture"
#endif


/*  Starts a new context with the given stack space by calling `func' with
    argument `arg'; the return value of which is returned to the caller of
    context_start() when the main function returns. */
void *context_start(void *stack, size_t size, void *(*main)(void*), void *arg);

/*  Restarts a saved context from saved state pointed to by `ptr', similar to
    what context_restore does, but also changes the return address for the main
    function so that control transfers back to the caller of context_restart()
    when the main function returns. */
void *context_restart(struct Context *ptr, void *arg, void *stack, size_t size);

/* Saves the execution context in *ptr and returns NULL if returning directly,
   or when restoring the context, the argument passed to context_restore(). */
void *context_save(struct Context *ptr);

/* Restores the execution context pointed to by `ptr'. Control does not pass
   back to the caller of context_restore(). */
void context_restore(struct Context *ptr, void *arg);

/* Retrieve the stack pointer in the current context */
void *context_sp();
