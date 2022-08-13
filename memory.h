#ifndef MEMORY_H
#define MEMORY_H

#define _alloc(s) malloc(s)
#define _alloc_ds(t) (t*)malloc(sizeof(t))
#define _alloc_ds_array(t, n) (t*)malloc(sizeof(t)*(n))
#define _realloc(p, s) realloc((p), (s))
#define _realloc_ds_array(p, t, n) (t*)realloc((p), sizeof(t)*(n))
#define _copy_str(s) strdup(s)
#define _free(p) free((void*)p)

#endif
