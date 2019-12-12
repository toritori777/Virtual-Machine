#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "lib/xalloc.h"
#include "lib/stack.h"
#include "lib/contracts.h"
#include "lib/c0v_stack.h"
#include "lib/c0vm.h"
#include "lib/c0vm_c0ffi.h"
#include "lib/c0vm_abort.h"

/* call stack frames */
typedef struct frame_info frame;
struct frame_info {
  c0v_stack_t S; /* Operand stack of C0 values */
  ubyte *P;      /* Function body */
  size_t pc;     /* Program counter */
  c0_value *V;   /* The local variables */
};


void push_int(c0v_stack_t S,int i){
  c0v_push(S,int2val(i));
  return;
}

int pop_int(c0v_stack_t S) {
  return val2int(c0v_pop(S));
}

void push_ptr(c0v_stack_t S,void* p){
  c0v_push(S,ptr2val(p));
  return;
}

void* pop_ptr(c0v_stack_t S) {
  return val2ptr(c0v_pop(S));
}

int execute(struct bc0_file *bc0) {
  REQUIRES(bc0 != NULL);

  /* Variables */
  c0v_stack_t S; /* Operand stack of C0 values */
  ubyte *P;      /* Array of bytes that make up the current function */
  size_t pc;     /* Current location within the current byte array P */
  c0_value *V;   /* Local variables (you won't need this till Task 2) */

  S=c0v_stack_new();
  P=bc0->function_pool->code;
  pc=0;
  V=xcalloc(bc0->function_pool->num_vars,sizeof(c0_value));

  /* The call stack, a generic stack that should contain pointers to frames */
  /* You won't need this until you implement functions. */
  gstack_t callStack;
  callStack=stack_new();

  while (true) {

#ifdef DEBUG
    /* You can add extra debugging information here */
    fprintf(stderr, "Opcode %x -- Stack size: %zu -- PC: %zu\n",
            P[pc], c0v_stack_size(S), pc);
#endif

    switch (P[pc]) {

    /* Additional stack operation: */

    case POP: {
      pc++;
      c0v_pop(S);
      break;
    }

    case DUP: {
      pc++;
      c0_value v = c0v_pop(S);
      c0v_push(S,v);
      c0v_push(S,v);
      break;
    }

    case SWAP: {
      pc++;
      c0_value v1=c0v_pop(S);
      c0_value v2=c0v_pop(S);
      c0v_push(S,v1);
      c0v_push(S,v2);
      break;
    }


    /* Returning from a function.
     * This currently has a memory leak! You will need to make a slight
     * change for the initial tasks to avoid leaking memory.  You will
     * need to be revise it further when you write INVOKESTATIC. */

    case RETURN: {
      c0_value retval=c0v_pop(S);
      assert(c0v_stack_empty(S));
#ifdef DEBUG
      fprintf(stderr, "Returning %d from execute()\n", val2int(retval));
#endif
      // Free everything before returning from the execute function!
      c0v_stack_free(S);
      free(V);

      if(stack_empty(callStack)){
        stack_free(callStack,NULL);
        return val2int(retval);
      }
      else{
        frame *F = (frame*)pop(callStack);
        S=F->S; 
        P=F->P;
        V=F->V;
        pc=F->pc;
        free(F);
        c0v_push(S,retval);
        break;
      }
    }


    /* Arithmetic and Logical operations */

    case IADD:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S,v1+v2);
      break;
    }

    case ISUB:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S,v2-v1);
      break;
    }

    case IMUL:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S,v1*v2);
      break;
    }

    case IDIV:{
      pc++;
      int v2=pop_int(S);
      int v1=pop_int(S);
      if(v2==0){
        c0_arith_error("Division by zero");
      }
      else if(v1==INT_MIN && v2==-1){
        c0_arith_error("Overflow");
      }
      push_int(S,v1/v2);
      break;
    }

    case IREM:{
      pc++;
      int v2=pop_int(S);
      int v1=pop_int(S);
      if(v2==0){
        c0_arith_error("Modulo by zero");
      }
      else if(v1==INT_MIN && v2==-1){
        c0_arith_error("Overflow");
      }
      push_int(S,v1%v2);
      break;

    }

    case IAND:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S,v1&v2);
      break;
    }

    case IOR:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S, v1|v2);
      break;
    }

    case IXOR:{
      pc++;
      int v1=pop_int(S);
      int v2=pop_int(S);
      push_int(S, v1^v2);
      break;
    }
 

    case ISHL:{
      pc++;
      int v2=pop_int(S);
      int v1=pop_int(S);
      if(v2<0||v2>31)
      {
        c0_arith_error("Division by zero");
      }
      push_int(S, v1<<v2);
      break;
    }

    case ISHR:{
      pc++;
      int v2=pop_int(S);
      int v1=pop_int(S);
      if(v2<0||v2>31)
      {
        c0_arith_error("Division by zero");
      }
      push_int(S, v1>>v2);
      break; 
    }


    /* Pushing constants */

    case BIPUSH:{
      pc++;
      push_int(S,(int)(int8_t)P[pc]);
      pc++;
      break;
    }

    case ILDC:{
      pc++;
      uint16_t indx1=((uint16_t)P[pc])<<8;
      pc++;
      uint16_t indx2=(uint16_t)P[pc];
      int x=bc0->int_pool[indx1|indx2];
      push_int(S, x);
      pc++;
      break;
    }

    case ALDC:{
      pc++;
      uint16_t indx=((uint16_t)P[pc])<<8;
      pc++;
      indx|=(uint16_t)P[pc];
      void *k=(void *)&(bc0->string_pool[indx]);
      c0v_push(S,ptr2val(k));
      pc++;
      break;
    }

    case ACONST_NULL:{
      pc++;
      push_ptr(S,NULL);
      break;
    }


    /* Operations on local variables */

    case VLOAD:{
      pc++;
      c0v_push(S,V[P[pc]]);
      pc++;
      break;
    }

    case VSTORE:{
      pc++;
      V[P[pc]]=c0v_pop(S);
      pc++;
      break;
    }


    /* Control flow operations */

    case NOP:{
      pc++;
      break;
    }

    case IF_CMPEQ:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S);
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(val_equal(v1,v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break; 
    }

    case IF_CMPNE:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S);
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(!val_equal(v1,v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break;
    }

    case IF_ICMPLT:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S); 
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(val2int(v1)<val2int(v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break;
    }

    case IF_ICMPGE:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S);
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(val2int(v1)>=val2int(v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break;
    }

    case IF_ICMPGT:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S);
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(val2int(v1)>val2int(v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break;
    }

    case IF_ICMPLE:{
      pc++;
      c0_value v2=c0v_pop(S);
      c0_value v1=c0v_pop(S);
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      if(val2int(v1)<=val2int(v2))
      {
        pc+=(int16_t)((o1<<8| o2)-2);
      }
      else
      {
        pc++;
      }
      break;
    }

    case GOTO:{
      pc++;
      int16_t o1=(int16_t)P[pc];
      pc++;
      int16_t o2=(int16_t)P[pc];
      pc+=(int16_t)((o1<<8| o2)-2);
      break;
    }

    case ATHROW:{
      pc++;
      c0_user_error((char*)pop_ptr(S));
      break;
    }

    case ASSERT:{
      pc++;
      char* a=(char*)pop_ptr(S);
      int v=pop_int(S);
      if(v==0)
      {
        c0_assertion_failure(a);
      }
      break;
    }


    /* Function call operations: */

    case INVOKESTATIC:{
      pc++;
      uint16_t o1=(uint16_t)P[pc];
      pc++;
      uint16_t o2=(uint16_t)P[pc];
      pc++;
      struct function_info fi=bc0->function_pool[o1<< 8| o2];
      frame* new=xmalloc(sizeof(frame));
      new->P=P;
      new->pc=pc; 
      new->V=V;
      new->S=S;
      push(callStack,(void*)new); 
      int length=(int)fi.num_vars;
      V=xcalloc(length,sizeof(c0_value));
      for (int i=fi.num_args-1; i>=0; i--) {
        V[i]=c0v_pop(S);
      }
      S=c0v_stack_new();
      P=fi.code;
      pc=0;
      break;
    }

 
    case INVOKENATIVE:{
      pc++;
      uint16_t o1=(uint16_t)P[pc];
      pc++;
      uint16_t o2=(uint16_t)P[pc];
      pc++;
      struct native_info ni=bc0->native_pool[o1<< 8| o2]; 
      uint16_t indx=ni.function_table_index;
      native_fn *f=native_function_table[indx];
      c0_value *v=xcalloc(ni.num_args,sizeof(c0_value));
      for (int i=ni.num_args-1; i>=0; i--) {
        v[i]=c0v_pop(S); 
      }
      c0v_push(S,(*f)(v));
      break;
    }

    /* Memory allocation operations: */

    case NEW:{
      pc++;
      int x=(int)(int8_t)P[pc];
      void *p=xmalloc(x);
      push_ptr(S,p);
      pc++;
      break;
    }
 
    case NEWARRAY:{
      pc++;
      int s=(int)(int8_t)P[pc];
      pc++;
      int n=pop_int(S);
      c0_array *arr=xmalloc(sizeof(c0_array));
      arr->count=n;
      arr->elt_size=s;
      arr->elems=xcalloc(n,s);
      push_ptr(S,(void*)arr);
      break;
    }

    case ARRAYLENGTH:{
      c0_array *arr=pop_ptr(S);
      push_int(S,arr->count);
      pc++;
      break; 
    }


    /* Memory access operations: */

    case AADDF:{
      pc++;
      ubyte f=(ubyte)P[pc];
      pc++;
      void *a=pop_ptr(S); 
      if(a==NULL){
        c0_memory_error("Trying to access NULL\n");
      }
      push_ptr(S,(ubyte*)a+f);
      break;
    }

    case AADDS:{
      pc++;
      int num=pop_int(S); 
      c0_array *arr=(c0_array*)pop_ptr(S);
      if(num<0 || num>=arr->count){
        c0_memory_error("Access out of bounds");
      } 
      if(arr==NULL){
        c0_memory_error("Trying to access NULL\n");
      } 
      push_ptr(S,(ubyte*)arr->elems+(num*(arr->elt_size)));
      break;
    }

    case IMLOAD:{
      pc++;
      int *a=pop_ptr(S);
      if(a==NULL){
        c0_memory_error("Trying to access NULL\n");
      }
      int x=*a;
      push_int(S,x);
      break;
    }

    case IMSTORE:{
      pc++;
      int x=pop_int(S);
      int *a=(int*)pop_ptr(S);
      if(a==NULL)
      {
        c0_memory_error("Trying to access NULL\n");
      }
      *a=x;
      break; 
    }

    case AMLOAD:{
      pc++;
      void **a=pop_ptr(S);
      if(a==NULL)
      {
        c0_memory_error("Trying to access NULL\n");
      }
      void *b=*a;
      push_ptr(S,b);
      break;
    }

    case AMSTORE:{
      pc++;
      void *b=pop_ptr(S);
      void **a=pop_ptr(S);
      if(a==NULL)
      {
        c0_memory_error("Trying to access NULL\n");
      }
      *a=b;
      break;
    }

    case CMLOAD:{
      pc++;
      char *a=pop_ptr(S);
      if(a==NULL)
      {
        c0_memory_error("Trying to access NULL\n");
      }
      push_int(S,(int)(*a));
      break;
    }
 
    case CMSTORE:{ 
      pc++;
      int x=pop_int(S);
      char *a=pop_ptr(S);
      if(a==NULL)
      {
        c0_memory_error("Trying to access NULL\n");
      }
      *a=x & 0x7F;
      break;
    }

    default:
      fprintf(stderr, "invalid opcode: 0x%02x\n", P[pc]);
      abort();
    }
  }

  /* cannot get here from infinite loop */
  assert(false);
}
