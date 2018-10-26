#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "smol.h"
int AP_PADDING = 0;

SMOL_STATE ss_make(char* program) {
    SMOL_STATE res;
    res.stack = al_make();
    res.program = program;
    res.ptr = 0;
    res.size = strlen(program);
    res.mode = SM_COLLECT;
    res.saved_mode = SM_STRING;
    return res;
}

int between(int64_t a, int64_t b, int64_t c) {
    return a <= b && b <= c;
}

ATOM ss_parse_op(SMOL_STATE* state, char op) {
    if(between('0', op, '9')) {
        return atom(op - '0');
    }
    else if(between('a', op, 'z')) {
        return atom(op - 'a' + 10);
    }
    else if(between('A', op, 'Z')) {
        return atom(op - 'A' + 36);
    }
    else if(op == '.') {
        return basic_list(0);
    }
    else if(op == '~') {
        return basic_list(1,    0L);
    }
    else if(op == '!') {
        return basic_list(2,    0L, 1L);
    }
    else if(op == '@') {
        return basic_list(3,    0L, 1L, 2L);
    }
    else if(op == '#') {
        return basic_list(4,    0L, 1L, 2L, 3L);
    }
    else if(op == '$') {
        return basic_list(5,    0L, 1L, 2L, 3L, 4L);
    }
    else if(op == '`') {
        return atom(state->program[++state->ptr]);
    }
    else {
        fprintf(stderr, "Unhandled op '%c' in `ss_parse_op`.\n", op);
        return atom(ERROR_VALUE);
    }
}

void ss_push(SMOL_STATE* state, ATOM a) {
    al_push(state->stack, a);
}

ATOM ss_pop(SMOL_STATE* state) {
    return al_pop(state->stack);
}

ATOM_NODE* ss_peek(SMOL_STATE* state) {
    return state->stack->tail;
}

#define SWAPTO(a, b)\
    case a:\
        mode = b;\
        break
void ss_swap_mode(SMOL_STATE* state) {
    SMOL_MODE mode;
    switch(state->mode) {
        SWAPTO(SM_COLLECT,  SM_EVALUATE);
        SWAPTO(SM_EVALUATE, SM_COLLECT);
        SWAPTO(SM_STRING,   SM_STRING);
    }
    state->mode = mode;
}
#undef SWAPTO

void ss_call(SMOL_STATE* state, char op) {
    if(between('0', op, '9')) {
        ss_push(state, ss_parse_op(state, op));
    }
    else switch(op) {
        // store register (quick)
        case 'X':
        case 'Y':
        case 'Z':
            state->registers[tolower(op)] = ss_pop(state);
            break;
        
        // store any register
        case 'R': {
            ATOM save_to, value;
            value = ss_pop(state);
            save_to = ss_pop(state);
            
            assert(save_to.type == AT_INTEGER);
            
            state->registers[save_to.val.ival] = value;
            break;
        }
        
        // restore register (quick)
        case 'x':
        case 'y':
        case 'z':
            ss_push(state, state->registers[(size_t) op]);
            break;
        
        // restore any register
        case 'r': {
            ATOM recall = ss_pop(state);
            
            assert(recall.type == AT_INTEGER);
            
            ss_push(state, state->registers[recall.val.ival]);
            break;
        }
        
        // print
        case 'p':
            atom_print(ss_pop(state));
            break;
        
        // debug
        case '?':
            puts("Debugging Smol state:");
            atom_print(atom_of_list(state->stack));
            break;
        
        // concat
        case ',': {
            ATOM first, second;
            second = enlist(ss_pop(state));
            first = enlist(ss_pop(state));
            
            assert(first.type == AT_LIST);
            assert(second.type == AT_LIST);
            
            ATOM_LIST* res = al_concat(first.val.lval, second.val.lval);
            
            ss_push(state, atom_of_list(res));
            break;
        }
        
        // NN+: add
        // A+:  sum
        case '+': {
            ATOM top = ss_pop(state);
            
            if(top.type == AT_INTEGER) {
                ATOM next = ss_pop(state);
                
                assert(next.type == AT_INTEGER);
                
                int64_t sum = next.val.ival + top.val.ival;
                
                ss_push(state, atom(sum));
            }
            else {
                fprintf(stderr, "Array summation is not supported.\n");
                
                ss_push(state, atom(ERROR_VALUE));
            }
            break;
        }
        
        // A@:  call
        case '@': {
            ATOM top = ss_pop(state);
            
            
            assert(top.type == AT_LIST);
            
            AL_ITER(iter, top.val.lval) {
                char to_op = iter->head.val.ival;
                ss_call(state, to_op);
            }
            
            break;
        }
        
        // A.:  puts
        // N.:  putchar
        case '.': {
            ATOM top = ss_pop(state);
            
            switch(top.type) {
                case AT_LIST:
                    AL_ITER(iter, top.val.lval) {
                        char el = (char) iter->head.val.ival;
                        putchar(el);
                    }
                    break;
                
                case AT_INTEGER:
                    putchar(top.val.ival);
                    break;
            }
            
            break;
        }
        
        
        case 'o': {
            ATOM top = ss_pop(state);
            
            assert(top.type == AT_LIST);
            
            al_pop(top.val.lval);
            
            ss_push(state, top);
            
            break;
        }
        
        // duplicate by reference
        case ':': {
            ATOM top = ss_pop(state);
            ss_push(state, top);
            ss_push(state, top);
            break;
        }
        // duplicate by value
        case 'd': {
            ATOM top = ss_pop(state);
            ss_push(state, top);
            ss_push(state, atom_copy(top));
            break;
        }
        
        default:
            fprintf(stderr, "Unrecognized character %c\n", op);
            break;
    }
}

void ss_step(SMOL_STATE* state) {
    char op = state->program[state->ptr];
    if(op == ';' && state->mode != SM_STRING) {
        ss_swap_mode(state);
    }
    else if(op == '\'') {
        if(state->saved_mode == SM_STRING) {
            state->saved_mode = state->mode;
            state->mode = SM_STRING;
            ss_push(state, basic_list(0));
        }
        else {
            state->mode = state->saved_mode;
            state->saved_mode = SM_STRING;
        }
    }
    else {
        switch(state->mode) {
            case SM_COLLECT:
                ss_push(state,
                    ss_parse_op(state, op));
                break;
            
            case SM_EVALUATE:
                ss_call(state, op);
                break;
            
            case SM_STRING: {
                ATOM_NODE* top = ss_peek(state);
                
                assert(top->head.type == AT_LIST);
                
                al_push(top->head.val.lval, atom(op));
                
                break;
            }
        }
    }
    state->ptr++;
}

void ss_run(SMOL_STATE* state) {
    while(state->ptr < state->size) {
        ss_step(state);
    }
}

ATOM_LIST* al_make() {
    ATOM_LIST* res = malloc(sizeof(ATOM_LIST));
    res->size = 0;
    res->head = NULL;
    res->tail = NULL;
    return res;
}
ATOM_NODE* an_make() {
    ATOM_NODE* res = malloc(sizeof(ATOM_NODE));
    return res;
}

void al_push(ATOM_LIST* list, ATOM val) {
    if(list->size == 0) {
        list->tail = an_make();
        list->tail->head = val;
        list->head = list->tail;
    }
    else {
        list->tail->next = an_make();
        list->tail->next->head = val;
        list->tail->next->prev = list->tail;

        list->tail = list->tail->next;
    }
    list->size++;
}

ATOM al_pop(ATOM_LIST* list) {
    ATOM_NODE* tail = list->tail;
    ATOM res = tail->head;

    if(list->size == 1) {
        list->head = NULL;
        list->tail = NULL;
    }
    else {
        
        // make 2nd to last element have no next element
        tail->prev->next = NULL;
        
        // update list's tail
        list->tail = tail->prev;
    }
    
    // destroy former tail
    // an_free(tail);

    list->size--;
    
    return res;
}

void al_iter(ATOM_LIST* list, ATOM_ITER_FN fn) {
    AL_ITER(iter, list) {
        fn(iter->head);
    }
}

ATOM_LIST* al_concat(ATOM_LIST* first, ATOM_LIST* second) {
    ATOM_LIST* res = al_make();
    
    AL_ITER(iter, first) {
        al_push(res, iter->head);
    }
    
    AL_ITER(iter, second) {
        al_push(res, iter->head);
    }
    
    return res;
}

ATOM_LIST* al_copy(ATOM_LIST* list) {
    ATOM_LIST* res = al_make();
    
    AL_ITER(iter, list) {
        al_push(res, iter->head);
    }
    
    return res;
}

ATOM atom_copy(ATOM source) {
    switch(source.type) {
        case AT_LIST:
            return atom_of_list(al_copy(source.val.lval));
        
        case AT_INTEGER:
            return atom(source.val.ival);
        
        default:
            fprintf(stderr, "Unhandled case in `atom_copy` %i.\n", source.type);
            return atom(ERROR_VALUE);
    }
}

ATOM atom(int64_t val) {
    ATOM res;
    res.val.ival = val;
    res.type = AT_INTEGER;
    return res;
}

ATOM basic_list(size_t cap, ...) {
    ATOM_LIST* list = al_make();
    va_list args;
    va_start(args, cap);
    
    for(size_t i = 0; i < cap; i++) {
        int64_t arg = va_arg(args, int64_t);
        al_push(list, atom(arg));
    }
    
    ATOM res;
    res.val.lval = list;
    res.type = AT_LIST;
    return res;
}

ATOM enlist(ATOM source) {
    switch(source.type) {
        case AT_INTEGER:
            return basic_list(1, source.val.ival);
        
        case AT_LIST:
            return source;
            
        default:
            fprintf(stderr, "Unhandled case in `enlist` %i.\n", source.type);
            return atom(ERROR_VALUE);
    }
}

ATOM atom_of_list(ATOM_LIST* list) {
    ATOM res;
    res.val.lval = list;
    res.type = AT_LIST;
    return res;
}

void put_padding() {
    printf("%*s", AP_PADDING, "");
}
void atom_print(ATOM atom) {
    put_padding();
    switch(atom.type) {
        case AT_INTEGER:
            printf("%"PRId64, atom.val.ival);
            break;
        
        case AT_LIST:
            puts("(");
            AP_PADDING += 4;
            al_iter(atom.val.lval, atom_print);
            AP_PADDING -= 4;
            put_padding();
            putchar(')');
            break;
    }
    putchar('\n');
}

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "Insufficient arguments passed to %s.", argv[0]);
        return 1;
    }
    
    SMOL_STATE state = ss_make(argv[1]);
    ss_run(&state);
    
}