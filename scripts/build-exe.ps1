gcc -static `
    src/fmi_client.c `
    src/fmiw_conf.c `
    src/main.c `
    -I ../lib/uo_httpc/include `
    -I ../lib/uo_cb/include `
    -I ../lib/uo_queue/include `
    -L ../lib/uo_httpc/lib `
    -L ../lib/uo_cb/lib `
    -L ../lib/uo_queue/lib `
    -l:libuo_httpc.a `
    -l:libuo_cb.a `
    -l:libuo_queue.a `
    -lws2_32 `
    -o bin/debug/fmi_client.exe `
    -g
