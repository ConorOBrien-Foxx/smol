#ifndef SMOL_INCL
#include <inttypes.h>
typedef union ATOM_VALUE {
    int64_t             ival;
    struct ATOM_LIST*   lval;
} ATOM_VALUE;

typedef enum ATOM_TYPE {
    AT_INTEGER, AT_LIST
} ATOM_TYPE;

typedef struct ATOM {
    ATOM_VALUE  val;
    ATOM_TYPE   type;
} ATOM;

typedef struct ATOM_NODE {
    ATOM                head;
    struct ATOM_NODE*   prev;
    struct ATOM_NODE*   next;
} ATOM_NODE;

typedef struct ATOM_LIST {
    ATOM_NODE*  head;
    ATOM_NODE*  tail;
    size_t      size;
} ATOM_LIST;

typedef enum SMOL_MODE {
    SM_COLLECT, SM_EVALUATE, SM_STRING,
} SMOL_MODE;

typedef struct SMOL_STATE {
    ATOM_LIST*  stack;
    char*       program;
    size_t      ptr;
    size_t      size;
    SMOL_MODE   mode;
    SMOL_MODE   saved_mode;
    ATOM        registers[256];
} SMOL_STATE;

typedef void (*ATOM_ITER_FN)(ATOM);

SMOL_STATE ss_make(char*);
int between(int64_t, int64_t, int64_t);
ATOM ss_parse_op(SMOL_STATE*, char);
void ss_call(SMOL_STATE*, char);
void ss_push(SMOL_STATE*, ATOM);
ATOM ss_pop(SMOL_STATE*);
ATOM_NODE* ss_peek(SMOL_STATE*);
void ss_swap_mode(SMOL_STATE*);
void ss_step(SMOL_STATE*);
void ss_run(SMOL_STATE*);

#define ERROR_VALUE (-1337L)

#define AL_ITER(var, of) \
    for(ATOM_NODE* var = of->head; var != NULL; var = var->next)

ATOM_LIST* al_make();
ATOM_NODE* an_make();
void al_push(ATOM_LIST*, ATOM);
ATOM al_pop(ATOM_LIST*);
ATOM_LIST* al_concat(ATOM_LIST*, ATOM_LIST*);
ATOM_LIST* al_copy(ATOM_LIST*);
void al_append(ATOM_LIST*, ATOM_LIST*);
void al_iter(ATOM_LIST*, ATOM_ITER_FN);
ATOM atom(int64_t);
ATOM atom_copy(ATOM);
ATOM basic_list(size_t cap, ...);
ATOM enlist(ATOM);
ATOM atom_of_list(ATOM_LIST*);
extern int AP_PADDING;
void atom_print(ATOM);




#endif