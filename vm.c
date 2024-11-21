#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

VM vm;

static Value
clockNative (int argCount, Value *args) {
  return NUMBER_VAL((double)clock () / CLOCKS_PER_SEC);
}

static void
resetStack () {
  vm.stackTop = vm.stack;
  vm.objects = NULL;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void
runtimeError (const char *format, ...) {
  CallFrame *frame;
  ObjFunction *function;
  size_t instruction;
  int i;
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (i = vm.frameCount -1; i >= 0; i--) {
    frame = &vm.frames[i];
    function = frame->closure->function;
    instruction = frame->ip - function->chunk.code - 1;
    fprintf (stderr, "[line %d] in ",
        function->chunk.lines[instruction]);
    if (NULL == function->name) {
      fprintf (stderr, "script\n");
    }
    else {
      fprintf (stderr, "%s()\n", function->name->chars);
    }
  }
  resetStack();
}

static void
defineNative (const char *name, NativeFn function) {
  push (OBJ_VAL(copyString (name, (int)strlen (name))));
  push (OBJ_VAL(newNative (function)));
  tableSet (&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop ();
  pop ();
}

void
initVM () {
  resetStack();
  initTable (&vm.globals);
  initTable (&vm.strings);
  defineNative ("clock", clockNative);
}

void
freeVM () {
  freeTable (&vm.globals);
  freeTable (&vm.strings);
  freeObjects ();
}

void
push (Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value
pop () {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value
peek (int distance) {
  return vm.stackTop[-1 - distance];
}

static bool
call (ObjClosure *closure, int argCount) {
  CallFrame *frame;
  if (closure->function->arity != argCount) {
    runtimeError ("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }
  if (FRAMES_MAX == vm.frameCount) {
    runtimeError ("Stack overflow.");
    return false;
  }
  frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool
callValue (Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_NATIVE:
        {
          NativeFn native = AS_NATIVE(callee);
          Value result = native (argCount, vm.stackTop - argCount);
          vm.stackTop -= argCount + 1;
          push (result);
          return true;
        }
      case OBJ_CLOSURE:
        {
          return call (AS_CLOSURE(callee), argCount);
        }
      default:
        break; /* Non-callable object type. */
    }
  }
  runtimeError ("Can only call functions and classes.");
  return false;
}

static ObjUpvalue *
captureUpvalue (Value *local) {
  ObjUpvalue *prevUpvalue = NULL, *createdUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;
  while (NULL != upvalue && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (NULL != upvalue && local == upvalue->location) {
    return upvalue;
  }
  createdUpvalue = newUpvalue (local);
  createdUpvalue->next = upvalue;
  if (NULL == prevUpvalue) {
    vm.openUpvalues = createdUpvalue;
  }
  else {
    prevUpvalue->next = createdUpvalue;
  }
  return createdUpvalue;
}

static void
closeUpvalues (Value *last) {
  while (NULL != vm.openUpvalues &&
        vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static bool
isFalsey (Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void
concatenate () {
  ObjString *b = AS_STRING(pop ());
  ObjString *a = AS_STRING(pop ());
  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy (chars, a->chars, a->length);
  memcpy (chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjString *result = takeString (chars, length);
  push (OBJ_VAL(result));
}

static InterpretResult
run () {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  Value *slot, aValue, bValue;
  ObjString *name;
  double aDouble, bDouble;
  uint8_t instruction, offset8;
  uint16_t offset16;

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
do { \
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
    runtimeError("Operands must be numbers.");  \
    return INTERPRET_RUNTIME_ERROR; \
  } \
  bDouble = AS_NUMBER(pop()); \
  aDouble = AS_NUMBER(pop()); \
  push (valueType(aDouble op bDouble)); \
} while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf(" ");
    for (slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT:
        {
          aValue = READ_CONSTANT();
          push (aValue);
          printf("\n");
          break;
        }
      case OP_NIL: push (NIL_VAL); break;
      case OP_TRUE: push (BOOL_VAL(true)); break;
      case OP_FALSE: push (BOOL_VAL(false)); break;
      case OP_POP: pop (); break;
      case OP_GET_LOCAL:
        {
          offset8 = READ_BYTE();
          push (frame->slots[offset8]);
          break;
        }
      case OP_SET_LOCAL:
        {
          offset8 = READ_BYTE();
          frame->slots[offset8] = peek (0);
          break;
        }
      case OP_GET_GLOBAL:
        {
          name = READ_STRING();
          if (!tableGet (&vm.globals, name, &aValue)) {
            runtimeError ("Undefined variable '%s'.", name->chars);
            return INTERPRET_RUNTIME_ERROR;
          }
          push (aValue);
          break;
        }
      case OP_DEFINE_GLOBAL:
        {
          name = READ_STRING();
          tableSet (&vm.globals, name, peek (0));
          pop ();
          break;
        }
      case OP_SET_GLOBAL:
        {
          name = READ_STRING();
          if (tableSet (&vm.globals, name, peek (0))) {
            tableDelete (&vm.globals, name);
            runtimeError ("Undefined variable '%s'.", name->chars);
            return INTERPRET_RUNTIME_ERROR;
          }
          break;
        }
      case OP_GET_UPVALUE:
        {
          offset8 = READ_BYTE();
          push(*frame->closure->upvalues[offset8]->location);
          break;
        }
      case OP_SET_UPVALUE:
        {
          offset8 = READ_BYTE();
          *frame->closure->upvalues[offset8]->location = peek (0);
          break;
        }
      case OP_EQUAL:
        {
          bValue = pop ();
          aValue = pop ();
          push (BOOL_VAL(valuesEqual (aValue, bValue)));
          break;
        }
      case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD:
        {
          if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
            concatenate ();
          }
          else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
            bDouble = AS_NUMBER(pop ());
            aDouble = AS_NUMBER(pop ());
            push (NUMBER_VAL(aDouble + bDouble));
          }
          else {
            runtimeError (
              "Operands must be two numbers or two strings.");
            return INTERPRET_RUNTIME_ERROR;
          }
          break;
        }
      case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
          push (BOOL_VAL(isFalsey (pop ())));
          break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError ("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push (NUMBER_VAL(-AS_NUMBER(pop ())));
        break;
      case OP_PRINT:
        {
          printValue (pop ());
          printf ("\n");
          break;
        }
      case OP_JUMP:
        {
          offset16 = READ_SHORT();
          frame->ip += offset16;
          break;
        }
      case OP_JUMP_IF_FALSE:
        {
          offset16 = READ_SHORT();
          if (isFalsey (peek (0))) { frame->ip += offset16; }
          break;
        }
      case OP_LOOP:
        {
          offset16 = READ_SHORT ();
          frame->ip -= offset16;
          break;
        }
      case OP_CALL:
        {
          int argCount = READ_BYTE();
          if (!callValue (peek (argCount), argCount)) {
            return INTERPRET_RUNTIME_ERROR;
          }
          frame = &vm.frames[vm.frameCount - 1];
          break;
        }
      case OP_CLOSURE:
        {
          ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
          ObjClosure *closure = newClosure (function);
          push (OBJ_VAL(closure));
          for (int i = 0; i < closure->upvalueCount; i++) {
            uint8_t isLocal = READ_BYTE();
            uint8_t index = READ_BYTE();
            if (isLocal) {
              closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
            }
            else {
              closure->upvalues[i] = frame->closure->upvalues[index];
            }
          }
          break;
        }
      case OP_CLOSE_UPVALUE:
        closeUpvalues (vm.stackTop - 1);
        pop ();
        break;
      case OP_RETURN:
        {
          Value result = pop ();
          closeUpvalues (frame->slots);
          vm.frameCount--;
          if (0 == vm.frameCount) {
            pop ();
            return INTERPRET_OK;
          }
          vm.stackTop = frame->slots;
          push (result);
          frame = &vm.frames[vm.frameCount - 1];
          break;
        }
    }
  }

#undef BINARY_OP
#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult
interpret (const char *source) {
  ObjClosure *closure;
  ObjFunction *function = compile (source);
  if (NULL == function) { return INTERPRET_COMPILE_ERROR; }
  push (OBJ_VAL(function));
  closure = newClosure (function);
  pop ();
  push (OBJ_VAL(closure));
  call (closure, 0);
  return run ();
}
