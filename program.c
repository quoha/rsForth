//
//  rsForth/main.c
//
//  Created by Michael Henderson on 4/22/14.
//
// ------------------------------------------------------------------------
//  a really simple Forth on top of a really simple VM
//
// interpret is a function that takes a stack with an input buffer on top
// it reads the next word on the stack and stuffs it into a page of the VM
// then executes the word. it keeps on looping until end of input.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ------------------------------------------------------------------------
typedef struct RSBUFFER    RSBUFFER;
typedef struct RSCELL      RSCELL;
typedef struct RSLEXEME    RSLEXEME;
typedef struct RSNODE      RSNODE;
typedef enum   RSNODEKIND  RSNODEKIND;
typedef enum   RSOPCODE    RSOPCODE;
typedef struct RSQUEUE     RSQUEUE;
typedef enum   RSQUEUEKIND RSQUEUEKIND;
typedef enum   RSRESULT    RSRESULT;
typedef struct RSVM        RSVM;
typedef struct RSWORD      RSWORD;

// ------------------------------------------------------------------------
enum RSNODEKIND {
    rskInvalid,
    rskBuffer,
    rskCell,
    rskInteger,
    rskNode,
    rskWord,
};

enum RSOPCODE {
    rsopNOOP = 0,
    rsopDBGDUMP,
    rsopEVAL,
    rsopHALT,
    rsopLITBUFFER,
    rsopLITINTEGER,
    rsopPOP,
    rsopPUSH,
    rsopRETURN,
    rsopWORD,
};

enum RSQUEUEKIND {
    rsqStack,
};

enum RSRESULT {
    rsrERROR,
    rsrHALT,
    rsrOKAY,
};

// ------------------------------------------------------------------------
struct RSBUFFER {
    char *curr;
    char *start;
    char *end;
    char  data[1];
};

struct RSCELL {
    union {
        RSBUFFER   *buffer;
        int         integer;
        char       *nameOfWord;
        const char *text;
    } data;
    RSOPCODE opcode;
};

struct RSLEXEME {
    enum {
        rslInteger,
        rslText,
    } kind;
    union {
        int  integer;
        char text[1];
    };
};

struct RSNODE {
    RSNODE    *left;
    RSNODE    *right;
    RSNODEKIND kind;
    union {
        RSBUFFER *buffer;
        RSCELL   *cell;
        int       integer;
        RSNODE   *node;
        RSWORD   *word;
    } data;
};

struct RSQUEUE {
    RSQUEUEKIND kind;
    int         numberOfNodes;
    RSNODE     *left;
    RSNODE     *right;
};

struct RSVM {
    RSCELL  *programCounter;
    RSQUEUE *bufferStack;
    RSQUEUE *dataStack;
    RSQUEUE *returnStack;
    RSCELL  *coreStart;
    RSCELL  *coreEnd;
    RSCELL  *dataStart;
    RSCELL  *nextAvailableCell;
    RSQUEUE *dictionary;
    int      debugLevel;
};

struct RSWORD {
    RSCELL *code;
    char    name[1];
};

// ------------------------------------------------------------------------
RSBUFFER *RS_Buffer_AllocFromCString(const char *string);
void      RS_Cell_SetBuffer(RSCELL *cell, RSBUFFER *buffer);
void      RS_Cell_SetEVAL(RSCELL *cell);
void      RS_Cell_SetHALT(RSCELL *cell);
void      RS_Cell_SetInteger(RSCELL *cell, int integer);
void      RS_Cell_SetNOOP(RSCELL *cell);
void      RS_Cell_SetRETURN(RSCELL *cell);
RSNODE   *RS_Node_Alloc(void);
RSNODE   *RS_Node_AllocBuffer(RSBUFFER *buffer);
RSNODE   *RS_Node_AllocCell(RSCELL *cell);
RSNODE   *RS_Node_AllocInteger(int integer);
RSNODE   *RS_Node_AllocWord(RSWORD *word);
RSQUEUE  *RS_Queue_Alloc(void);
RSNODE   *RS_Queue_Pop(RSQUEUE *q);
RSQUEUE  *RS_Queue_Push(RSQUEUE *q, RSNODE *node);
int       RS_Queue_Size(RSQUEUE *q);
RSVM     *RS_VM_Alloc(void);
RSRESULT  RS_VM_CompileString(RSVM *vm, const char *code);
void      RS_VM_DictionaryAdd(RSVM *vm, const char *nameOfWord, RSWORD *word);
RSWORD   *RS_VM_DictionaryLookup(RSVM *vm, const char *nameOfWord);
void      RS_VM_LoadCore(RSVM *vm);
void      RS_VM_PushBuffer(RSVM *vm, RSBUFFER *buffer);
RSRESULT  RS_VM_Step(RSVM *vm);

// ------------------------------------------------------------------------
int main(int argc, const char * argv[]) {
    RSVM *vm = RS_VM_Alloc();
    RS_VM_LoadCore(vm);

    // add some code to test
    RS_VM_CompileString(vm, "dump.pc noop 42 hugs dump.pc halt");

    RSRESULT r;
    while ((r = RS_VM_Step(vm)) == rsrOKAY) {
        ;
    }

    printf("...vm:\tfinal result %d\n", r);

    RSBUFFER *input = RS_Buffer_AllocFromCString("42 .");
    RS_VM_PushBuffer(vm, input);

    vm->programCounter    = vm->dataStart;
    vm->nextAvailableCell = vm->dataStart;

    RS_Cell_SetEVAL(vm->nextAvailableCell++);
    RS_Cell_SetHALT(vm->nextAvailableCell++);

    while ((r = RS_VM_Step(vm)) == rsrOKAY) {
        ;
    }
    printf("...vm:\tfinal result %d\n", r);

    return 0;
}

// ------------------------------------------------------------------------
RSBUFFER *RS_Buffer_AllocFromCString(const char *string) {
    string = string ? string : "";
    ssize_t length = strlen(string);
    RSBUFFER *b = malloc(sizeof(*b) + length);
    if (!b) {
        perror(__FUNCTION__);
        exit(2);
    }
    b->start = b->data;
    b->end   = b->data + length;
    memcpy(b->data, string, length);
    b->data[length] = 0;
    return b;
}

// ------------------------------------------------------------------------
void RS_Cell_SetBuffer(RSCELL *cell, RSBUFFER *buffer) {
    if (cell) {
        cell->data.buffer = buffer;
        cell->opcode      = rsopLITBUFFER;
    }
}

// ------------------------------------------------------------------------
void RS_Cell_SetEVAL(RSCELL *cell) {
    if (cell) {
        cell->opcode = rsopEVAL;
    }
}

// ------------------------------------------------------------------------
void RS_Cell_SetHALT(RSCELL *cell) {
    if (cell) {
        cell->opcode = rsopHALT;
    }
}

// ------------------------------------------------------------------------
void RS_Cell_SetInteger(RSCELL *cell, int integer) {
    if (cell) {
        cell->data.integer = integer;
        cell->opcode       = rsopLITINTEGER;
    }
}

// ------------------------------------------------------------------------
void RS_Cell_SetNOOP(RSCELL *cell) {
    if (cell) {
        cell->opcode = rsopNOOP;
    }
}

// ------------------------------------------------------------------------
RSNODE *RS_Node_Alloc(void) {
    RSNODE *node = malloc(sizeof(*node));
    if (!node) {
        perror(__FUNCTION__);
        exit(2);
    }
    node->left = node->right = 0;
    node->kind = rskInvalid;
    return node;
}

// ------------------------------------------------------------------------
RSNODE *RS_Node_AllocBuffer(RSBUFFER *buffer) {
    RSNODE *node       = RS_Node_Alloc();
    node->kind         = rskBuffer;
    node->data.buffer  = buffer;
    return node;
}

// ------------------------------------------------------------------------
RSNODE *RS_Node_AllocCell(RSCELL *cell) {
    RSNODE *node       = RS_Node_Alloc();
    node->kind         = rskCell;
    node->data.cell    = cell;
    return node;
}

// ------------------------------------------------------------------------
RSNODE *RS_Node_AllocInteger(int integer) {
    RSNODE *node       = RS_Node_Alloc();
    node->kind         = rskInteger;
    node->data.integer = integer;
    return node;
}

// ------------------------------------------------------------------------
RSNODE *RS_Node_AllocWord(RSWORD *word) {
    RSNODE *node       = RS_Node_Alloc();
    node->kind         = rskWord;
    node->data.word    = word;
    return node;
}

// ------------------------------------------------------------------------
RSQUEUE *RS_Queue_Alloc(void) {
    RSQUEUE *q = malloc(sizeof(*q));
    if (!q) {
        perror(__FUNCTION__);
        exit(2);
    }
    q->kind            = rsqStack;
    q->numberOfNodes   = 0;
    q->left = q->right = 0;
    return q;
}

// ------------------------------------------------------------------------
RSNODE *RS_Queue_Pop(RSQUEUE *q) {
    if (!q) {
        printf("error:\t%s %s %d\n\tpopping from invalid queue\n", __FILE__, __FUNCTION__, __LINE__);
        exit(2);
    }

    RSNODE *node = 0;

    if (q->numberOfNodes == 0) {
        // empty queue
    } else if (q->numberOfNodes == 1) {
        // will be empty after the pop
        //
        node = q->left;
        q->left = q->right = 0;
        q->numberOfNodes = 0;
    } else {
        // will not be empty after the pop
        //
        switch (q->kind) {
            case rsqStack:
                node = q->right;
                q->right = q->right->left;
                q->right->right = 0;
                break;
        }
        node->left = node->right = 0;
        q->numberOfNodes--;
    }

    return node;
}

// ------------------------------------------------------------------------
RSQUEUE *RS_Queue_Push(RSQUEUE *q, RSNODE *node) {
    if (!q) {
        printf("error:\t%s %s %d\n\tpushing to invalid queue\n", __FILE__, __FUNCTION__, __LINE__);
        exit(2);
    }
    if (!node) {
        printf("error:\t%s %s %d\n\tpushing invalid node\n", __FILE__, __FUNCTION__, __LINE__);
        exit(2);
    }

    if (q->numberOfNodes == 0) {
        // empty queue
        //
        q->left = q->right = node;
    } else {
        // append the node to the queue
        //
        switch (q->kind) {
            case rsqStack:
                node->left  = q->right;
                node->right = 0;
                q->right->right = node;
                q->right        = node;
                break;
        }
    }

    q->numberOfNodes++;

    return q;
}

// ------------------------------------------------------------------------
int RS_Queue_Size(RSQUEUE *q) {
    return q ? q->numberOfNodes : 0;
}

// ------------------------------------------------------------------------
RSVM *RS_VM_Alloc(void) {
    RSVM *vm = malloc(sizeof(*vm));
    if (!vm) {
        perror(__FUNCTION__);
        exit(2);
    }

    vm->debugLevel  = 0;
    vm->bufferStack = RS_Queue_Alloc();
    vm->dataStack   = RS_Queue_Alloc();
    vm->dictionary  = RS_Queue_Alloc();
    vm->returnStack = RS_Queue_Alloc();

    int numberOfCells = 4 * 1024;
    vm->coreStart = malloc(sizeof(RSCELL) * (numberOfCells));
    if (!vm->coreStart) {
        perror(__FUNCTION__);
        exit(2);
    }
    vm->coreEnd = vm->coreStart + numberOfCells;

    vm->dataStart = vm->coreStart;

    // initialize memory
    //
    for (vm->nextAvailableCell = vm->coreStart; vm->nextAvailableCell < vm->coreEnd; vm->nextAvailableCell++) {
        RS_Cell_SetHALT(vm->nextAvailableCell);
    }

    // set the cell pointers to the beginning of memory
    //
    vm->programCounter    = vm->dataStart;
    vm->nextAvailableCell = vm->dataStart;

    return vm;
}

// ------------------------------------------------------------------------
RSRESULT RS_VM_CompileString(RSVM *vm, const char *code) {
    struct codes {
        const char *op;
        RSOPCODE   code;
    } codes[] = {
        {"dump.pc"   , rsopDBGDUMP},
        {"halt"      , rsopHALT},
        {"noop"      , rsopNOOP},
        {"return"    , rsopRETURN},
        {0           , rsopNOOP},
    };
    while (*code) {
        if (isspace(*code)) {
            while (isspace(*code)) {
                code++;
            }
            continue;
        }
        const char *op = code;
        while (*code && !isspace(*code)) {
            code++;
        }
        ssize_t len = code - op;
        if (*code) {
            code++;
        }
        if (isdigit(*op)) {
            vm->nextAvailableCell->opcode = rsopLITINTEGER;
            vm->nextAvailableCell->data.integer = atoi(op);
            vm->nextAvailableCell++;
        } else {
            // find code in table
            int idx;
            for (idx = 0; codes[idx].op; idx++) {
                if (!strncmp(op, codes[idx].op, len)) {
                    break;
                }
            }
            if (codes[idx].op) {
                vm->nextAvailableCell->opcode = codes[idx].code;
                vm->nextAvailableCell++;
            } else {
                vm->nextAvailableCell->opcode = rsopWORD;
                vm->nextAvailableCell->data.nameOfWord = strndup(op, len);
                vm->nextAvailableCell++;
            }
        }
    }
    return *code ? rsrERROR : rsrOKAY;
}

// ------------------------------------------------------------------------
void RS_VM_Define(RSVM *vm, const char *nameOfWord, RSCELL *code) {
    RSWORD *word = malloc(sizeof(*word) + strlen(nameOfWord));
    if (!word) {
        perror(__FUNCTION__);
        exit(2);
    }
    word->code = code;
    strcpy(word->name, nameOfWord);

    RS_VM_DictionaryAdd(vm, nameOfWord, word);
}

// ------------------------------------------------------------------------
void RS_VM_DictionaryAdd(RSVM *vm, const char *nameOfWord, RSWORD *word) {
    printf(".warn:\t%s %s %d\n\tDictionaryAdd(vm, %s, word)\n", __FILE__, __FUNCTION__, __LINE__, nameOfWord);

    if (vm && nameOfWord && *nameOfWord && word) {
        RS_Queue_Push(vm->dictionary, RS_Node_AllocWord(word));
    }
}

// ------------------------------------------------------------------------
RSWORD *RS_VM_DictionaryLookup(RSVM *vm, const char *nameOfWord) {
    RSNODE *n = vm->dictionary->right;
    while (n) {
        if (!strcmp(n->data.word->name, nameOfWord)) {
            printf(".warn:\t%s %s %d\n\tDictionaryLookup(vm, %s) -- found word\n", __FILE__, __FUNCTION__, __LINE__, nameOfWord);
            return n->data.word;
        }
        n = n->left;
    }
    printf(".warn:\t%s %s %d\n\tDictionaryLookup(vm, %s) -- no such word\n", __FILE__, __FUNCTION__, __LINE__, nameOfWord);
    return 0;
}

// ------------------------------------------------------------------------
// define some core words. leave the program counter pointing to the memory
// after the words. update the data pointer, too, so that future users don't
// harm the core words.
//
void RS_VM_LoadCore(RSVM *vm) {
    RSCELL *startOfCode = vm->nextAvailableCell;
    RS_VM_CompileString(vm, "dump.pc return");
    RS_VM_Define(vm, "hugs", startOfCode);

    startOfCode = vm->nextAvailableCell;
    RS_VM_CompileString(vm, "dump.pc halt return");
    RS_VM_Define(vm, "eval", startOfCode);

    vm->programCounter = vm->nextAvailableCell;
    vm->dataStart      = vm->nextAvailableCell;
}

// ------------------------------------------------------------------------
void RS_VM_PushBuffer(RSVM *vm, RSBUFFER *buffer) {
    if (vm && buffer) {
        RS_Queue_Push(vm->bufferStack, RS_Node_AllocBuffer(buffer));
    }
}

// ------------------------------------------------------------------------
// execute a single instruction
//
RSRESULT RS_VM_Step(RSVM *vm) {
    // check range of program counter
    //
    if (!(vm->coreStart <= vm->programCounter && vm->programCounter < vm->coreEnd)) {
        printf("error:\t%s %s %d\n\tprogramCounter out of range\n\t%-18s == %p\n\t%-18s == %p\n\t%-18s == %p\n", __FILE__, __FUNCTION__, __LINE__, "programCounter", vm->programCounter, "coreStart", vm->coreStart, "coreEnd", vm->coreEnd);
        exit(2);
    }

    // grab the cell that the program counter is pointing to and execute it
    //
    RSNODE *n;
    RSCELL *cell = vm->programCounter++;
    RSWORD *wordToExecute;
    switch (cell->opcode) {
        case rsopDBGDUMP:
            printf("...vm:\tpc = %5ld -- dbgdump\n", cell - vm->coreStart);
            return rsrOKAY;
        case rsopEVAL:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- eval\n", cell - vm->coreStart);
            }
            // verify stack size
            if (RS_Queue_Size(vm->bufferStack) < 1) {
                printf("...vm:\tEVAL requires at least one buffer on the input stack\n");
                exit(2);
            }
            // pull the next word from the buffer
            // execute the word
            exit(2);
        case rsopHALT:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- halting\n", cell - vm->coreStart);
            }
            // reset the program counter back to the cell with the HALT
            // instruction so that future calls to the VM will also halt
            //
            vm->programCounter = cell;
            return rsrHALT;
        case rsopLITBUFFER:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- literal buffer %p\n", cell - vm->coreStart, cell->data.buffer);
            }
            // push the literal in the cell onto the stack
            //
            RS_Queue_Push(vm->dataStack, RS_Node_AllocBuffer(cell->data.buffer));
            return rsrOKAY;
        case rsopLITINTEGER:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- literal integer %d\n", cell - vm->coreStart, cell->data.integer);
            }
            // push the literal in the cell onto the stack
            //
            RS_Queue_Push(vm->dataStack, RS_Node_AllocInteger(cell->data.integer));
            return rsrOKAY;
        case rsopNOOP:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- no-op\n", cell - vm->coreStart);
            }
            return rsrOKAY;
        case rsopPOP:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- pop -- not implemented\n", cell - vm->coreStart);
            }
            exit(2);
        case rsopPUSH:
            printf("...vm:\tpc = %5ld -- push -- not implemented\n", cell - vm->coreStart);
            exit(2);
        case rsopRETURN:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- return\n", cell - vm->coreStart);
            }
            // set the program counter to the value on the return stack
            //
            n = RS_Queue_Pop(vm->returnStack);
            if (!n) {
                printf("error:\tpc = %5ld -- return with empty RS\n", cell - vm->coreStart);
                exit(2);
            }
            vm->programCounter = n->data.cell;
            // free node
            free(n);
            return rsrOKAY;
        case rsopWORD:
            if (vm->debugLevel) {
                printf("...vm:\tpc = %5ld -- word\n", cell - vm->coreStart);
            }
            // lookup the word in the dictionary
            wordToExecute = RS_VM_DictionaryLookup(vm, cell->data.nameOfWord);
            if (!wordToExecute) {
                printf("error:\tundefined word '%s'\n", cell->data.nameOfWord);
                exit(2);
            }
            // push the current program counter
            RS_Queue_Push(vm->returnStack, RS_Node_AllocCell(vm->programCounter));
            // set the program counter to the word
            vm->programCounter = wordToExecute->code;
            // and let it run. we depend on the word to do a return at the end
            return rsrOKAY;
    }
    printf("error:\t%s %s %d\n\tinvalid memory cell encountered - halting\n\t%-18s == %ld\n\t%-18s == %d\n", __FILE__, __FUNCTION__, __LINE__, "programCounter", cell - vm->coreStart, "opcode", cell->opcode);
    exit(2);
    return rsrERROR;
}
