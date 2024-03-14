// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct {
  struct spinlock lock;
  struct buf head;
} bucket[NBUCKET];

// Add buf to bucket, need to hold bucket lock
static void 
addbuf(struct buf* b, int ibucket) 
{
  struct buf *next;

  next = bucket[ibucket].head.next;
  bucket[ibucket].head.next = b;
  b->next = next;
}

// Add buf from bucket, need to hold bucket lock
static void
rmbuf(struct buf* b, int ibucket) 
{
  struct buf* p = &bucket[ibucket].head;
  while (p->next != b) {
    p = p->next;
    if (p == 0)
      panic("brelse: rmbuf");
  }
  p->next = b->next;
  b->next = 0;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; ++i) 
    initlock(&bucket[i].lock, "bcache bucket");

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
    initsleeplock(&b->lock, "buffer");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int i = blockno % NBUCKET;

  acquire(&bucket[i].lock);

  // Is the block already cached?
  for (b = bucket[i].head.next; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.lock);

  // Not cached.
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      addbuf(b, i);
      release(&bcache.lock);
      release(&bucket[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int i = b->blockno % NBUCKET;

  releasesleep(&b->lock);
  
  acquire(&bucket[i].lock);
  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0)  // no one is waiting for it.
    rmbuf(b, i);
  release(&bcache.lock);
  release(&bucket[i].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}