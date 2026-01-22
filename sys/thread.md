# Implementation requirements

All functions must meet the requires noted in their description that follows a
function declaration in thread.h. In addition, an implementation must satisfy
the requirements given below. The terms _must_, _must not_, _shall_, _shall
not_, _should_, _should not_, and _may_ have the following precise meanings.

- <ins>_Must_</ins> and <ins>_shall_</ins> mean that an implementation is required to have the
  described behavior.
- <ins>_Must not_</ins> and <ins>_shall not_</ins> mean that an implementation is prohibited from
  exhibiting the described behavior.
- <ins>_Should_</ins> means that the described behavior is recommended but not required.
- <ins>_Should not_</ins> means that the described behavior  is discouraged but not
  prohibited.
- <ins>_May_</ins> means that the described behavior is neither encouraged nor discouraged.

## spawn_thread()

- <ins>Must</ins> allocate a stack of size `HEAP_ALLOC_MAX` using *kmalloc()*
  prior to MP3cp2.
- <ins>Must</ins> allocate a stack of size `PAGE_SIZE` *alloc_phys_pages()*
  starting with MP3cp2.
- <ins>Must not</ins> switch to another thread context.

# Optional features