#include "pool.h"
#include <stdio.h> /* for fprintf */
#include <stdlib.h> /* for calloc, free, etc.. */
#include <string.h> /* for memcpy/memset */
#include <assert.h> /* for assert */

/* Pool data is following:
 * exist byte + item data
 * [1I|1I|0I|1I|1I|1I] */

enum { RETURN_OK = 1, RETURN_FAIL = 0 };

typedef struct _chckPool {
   unsigned char *buffer;
   size_t step, used, allocated, member, items;
} _chckPool;

static int chckPoolResize(chckPool *pool, size_t size)
{
   void *tmp = NULL;
   assert(size != 0 && size != pool->allocated);

   if (pool->buffer && pool->allocated < size)
      tmp = realloc(pool->buffer, size);

   if (!tmp) {
      if (!(tmp = malloc(size)))
         return RETURN_FAIL;

      if (pool->buffer) {
         memcpy(tmp, pool->buffer, (pool->used < size ? pool->used : size));
         free(pool->buffer);
      }
   }

   memset(tmp + pool->allocated, 0, size);
   pool->buffer = tmp;
   pool->allocated = size;
   return RETURN_OK;
}

chckPool* chckPoolNew(size_t growStep, size_t initialItems, size_t memberSize)
{
   chckPool *pool = NULL;
   assert(memberSize > 0);

   if (!memberSize)
      goto fail;

   if (!(pool = calloc(1, sizeof(chckPool))))
      goto fail;

   pool->member = memberSize + 1;
   pool->step = (growStep ? growStep : 32);

   if (initialItems > 0)
      chckPoolResize(pool, initialItems * pool->member);

   return pool;

fail:
   if (pool)
      chckPoolFree(pool);
   return 0;
}

void chckPoolFree(chckPool *pool)
{
   assert(pool);
   chckPoolFlush(pool);
   free(pool);
}

void chckPoolFlush(chckPool *pool)
{
   assert(pool);

   if (pool->buffer)
      free(pool->buffer);

   pool->buffer = NULL;
   pool->allocated = pool->used = 0;
}

size_t chckPoolCount(const chckPool *pool)
{
   assert(pool);
   return pool->items;
}

void* chckPoolGet(const chckPool *pool, chckPoolItem item)
{
   assert(item < pool->used);
   return (item ? pool->buffer + item : NULL);
}

void* chckPoolGetAt(const chckPool *pool, size_t index)
{
   void *item;
   size_t iter, items;

   for (iter = 0, items = 0; (item = chckPoolIter(pool, &iter, NULL)); ++items) {
      if (index == items)
         return item;
   }

   return NULL;
}

chckPoolItem chckPoolAdd(chckPool *pool, size_t size)
{
   assert(pool);
   assert(size + 1 == pool->member);

   if (size + 1 != pool->member) {
      fprintf(stderr, "chckPoolAdd: size should be same as member size when pool was created");
      return 0;
   }

   size_t next;
   for (next = 0; next < pool->used && *(pool->buffer + next) == 1; next += pool->member);

   if (pool->allocated < next + pool->member && chckPoolResize(pool, pool->allocated + pool->member * pool->step) != RETURN_OK)
      return 0;

   *(pool->buffer + next) = 1; // exist

   if (next + pool->member > pool->used)
      pool->used = next + pool->member;

   pool->items++;
   return next + 1;
}

void* chckPoolAddEx(chckPool *pool, size_t size, chckPoolItem *item)
{
   chckPoolItem i = chckPoolAdd(pool, size);

   if (item)
      *item = i;

   return chckPoolGet(pool, i);
}

void chckPoolRemove(chckPool *pool, chckPoolItem item)
{
   assert(pool);
   assert(item > 0 && item < pool->used);

   if (item >= pool->used)
      return;

   memset(pool->buffer + item - 1, 0, pool->member);

   if (item - 1 + pool->member >= pool->used)
      pool->used -= pool->member;

   if (pool->used + pool->member * pool->step < pool->allocated)
      chckPoolResize(pool, pool->allocated - pool->member * pool->step);

   pool->items--;
}

void* chckPoolIter(const chckPool *pool, size_t *iter, chckPoolItem *item)
{
   unsigned char *current;
   assert(pool && iter);

   if (item)
      *item = 0;

   if (*iter >= pool->used)
      return NULL;

   do {
      if (item)
         *item = *iter + 1;
      current = pool->buffer + *iter;
      *iter += pool->member;
   } while (*iter < pool->used && *current == 0);

   return (*current == 0 ? NULL : current + 1);
}

void chckPoolIterCall(const chckPool *pool, void (*function)(void *item))
{
   assert(pool);

   size_t iter;
   void *current;
   for (iter = 0; (current = chckPoolIter(pool, &iter, NULL));)
      function(current);
}

/* vim: set ts=8 sw=3 tw=0 :*/
