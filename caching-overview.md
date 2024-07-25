How caching works in nsxiv:


- main
    - tns_load if --bg-cache
    - static void run
        - Thumbs are loaded one app-loop at a time (so as to not hang?)
        - if init_thumb (se tem q fazer cache da thumb) tns_load
        - if load_thumb (se tem q desenhar thumb) tns_load

    - tns_load
        - if caching (!force) ->
            - try and load from cache (tns_cache_load)
                - stat cached image
                - stat current image
                - either load img or (out) set force to true
        - img_open:
            - access (syscall)
            - stat
            - imlib_load_image_frame
        - if not cache_hit:
            - tns_scale_down (p tamanho máximo do cache)
            - tns_cache_write
        - tns_scale_down (p tamanho zoom atual)
            - dúvida: se fizer downscale da imagem menor, do cache, é mais rápido?
        - advance initnext and loadnext

# The plan:

1. Have a global priority queue
2. put jobs in the queue (load and init)
3. have a thread pool to consume jobs from the queue

# Other features planned:

1. CLI switch to keep offscreen photos loaded
