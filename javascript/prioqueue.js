
// PrioQueue supports adding and removing values in descending priority order.
// Removing a value returns the value with the highest priority from all contained values.
// The default comparison function assumes the lowest value to be of highest priority.
// Supported operations are: insert(), remove(), merge(), clear().
// Supported queries are: val, length, comparison.
// Implementation uses a heap.
// Idea is from https://en.wikipedia.org/wiki/Fibonacci_heap
// A list of subheaps is maintained.
// Every subheap has a size equal to a power of two.
// For every size there exists exactly one or no heap.

// const PrioQueue = function(){ // own namespace ?

const defaultComparison=(v1,v2) => v1<v2

// action: initializes a new PrioQueue object
// param higherPrio: comparison function to determine value with higher priority.
//                   Signature is (v1,v2) => boolean.
//                   A return value of true means v1 has higher priority than v2.
//                   A return value of false means v1 has equal or lower priority than v2.
// returns: initialized PrioQueue object
function PrioQueue(higherPrio=defaultComparison) {
   this.heaps = [] // this.heaps[i] contains RootNode of heap with size 2**i or undefined
   this.higherPrio = higherPrio // comparison function (v1,v2) returning true if v1 has higher priority than v2 else false
   this.prioheap = undefined // points to rootnode of heap with highest priority value
   this.size = 0 // number of stored values
}

//////////////////////////////
// Private Helper Functions //
//////////////////////////////

// action: builds new heap with size = 1 / logsize = 0 (2**logsize == 1)
// returns: An initialized root node of a heap. It contains an empty list of sub-heaps (childs).
//          A root node of size 2**i contains a complete list of childs with smaller sizes 2**0 ... 2**(i-1).
// invariant: The size of childs is 1+2+4+8+...+2**(logsize-1)
// invariant: The size of this heap is size(childs) + 1/*root node*/ == 2**logsize
// invariant: this.childs[i] is defined if 0<=i&&i<this.logsize else undefined
// invariant: if this.childs[i] is defined then this.childs[i].logsize == i
// invariant: values of all childs and sub-childs have less or equal priority
function RootNode(value) {
   this.childs = [] // this.childs[i] contains RootNode of sub-heap with size 2**i
   this.logsize = 0 // size of heap is 1 (=== 2**logsize)
   this.value = value // value which has highest priority compared to values of all childs and their childs(heap condition).
}

// precondition: heap1 and heap2 must have same size 2**i (same logsize) and be different.
// param _this: points to PrioQueue
// param heap1: RootNode of one heap
// param heap2: RootNode of another heap
// action: concatenates heap1 and heap2 to form a heap with size 2**(i+1)
// returns: concatenated heap which preserves heap condition (rootnode has value with highest priority compared to values of all sub-heaps)
function concatHeaps(_this, heap1, heap2) {
   if (_this.higherPrio(heap2.value, heap1.value)) {
      [heap1, heap2] = [heap2, heap1]  // now heap1 contains value with highest priority
   }
   ++ heap1.logsize
   heap1.childs.push(heap2)
   return heap1
}

// precondition: _this.heaps[logsize] is defined && newHeap.logsize === logsize
// param _this: points to PrioQueue
// param logsize: logsize of newHeap
// param newHeap: RootNode of heap which is concatenated to _this.heaps[logsize]
// action: concatenates _this.heaps[logsize] with newHeap to form a heap with size 2**(logsize+1)
//         _this.heaps[logsize] is set to undefined
// returns: concatenated heap which preserves heap condition
function concat(_this, logsize, newHeap) {
   const mergedHeap = concatHeaps(_this, _this.heaps[logsize], newHeap)
   _this.heaps[logsize] = undefined
   return mergedHeap
}

// precondition: logsize === newHeap.logsize
// param _this: points to PrioQueue
// param logsize: logsize of newHeap
// param newHeap: RootNode of heap which is added to _this.heaps
// action: Puts heap into _this.heaps[logsize] if slot is empty
//         or concatenates newHeap with _this.heaps[logsize]
//         and tries to put concatenated heap into slot _this.heaps[logsize+1] recursively
// returns: undefined
function addHeap(_this, logsize, newHeap)
{
   if (_this.heaps[logsize] === undefined) {
      _this.heaps[logsize] = newHeap
   }
   else {
      const mergedHeap = concat(_this, logsize, newHeap)
      addHeap(_this, logsize+1, mergedHeap)
   }
}

///////////////////////////////////
// Public Interface of PrioQueue //
///////////////////////////////////

// action: adds new value to this Queue in worst-case Big-O(log(this.size)) (amortized in O(1))
// returns: this
PrioQueue.prototype.insert = function(value) {
   const newHeap = new RootNode(value)
   ++ this.size
   if (  this.prioheap === undefined
      || this.higherPrio(newHeap.value, this.prioheap.value)) {
      this.prioheap = newHeap
   }
   addHeap(this, 0, newHeap)
   return this
}

// action: removes value with highest priority from Queue in Big-O(log(this.size)) or does nothing if empty
// returns: removed value or undefined if empty
PrioQueue.prototype.remove = function() {
   if (this.size === 0) {
      return undefined
   }
   -- this.size
   const value = this.prioheap.value
   const childs = this.prioheap.childs
   this.heaps[this.prioheap.logsize] = undefined
   this.prioheap = undefined
   for (var logsize=0; logsize < childs.length; ++logsize) {
      if (this.heaps[logsize] === undefined)
         this.heaps[logsize] = childs[logsize]
      else {
         var mergedHeap = concat(this, logsize, childs[logsize])
         for (++logsize; logsize < childs.length; ++logsize) {
            mergedHeap = concatHeaps(this, childs[logsize], mergedHeap)
         }
         addHeap(this, logsize, mergedHeap)
      }
   }
   for (var logsize=0; logsize < this.heaps.length; ++logsize) {
      if (this.heaps[logsize] !== undefined
         && (this.prioheap === undefined
            || this.higherPrio(this.heaps[logsize].value, this.prioheap.value)))
         this.prioheap = this.heaps[logsize]
   }
   // shrink this.heaps to keep array size in Big-O(log(this.size))
   while (this.heaps.length && this.heaps[this.heaps.length-1] === undefined)
      this.heaps.pop()
   return value
}

// action: removes all elements from queue and adds them to this Queue in time Big-O(log(heap2.size))
// returns: undefined
PrioQueue.prototype.merge = function(queue) {
   if (this === queue)
      return
   if (this.higherPrio != queue.higherPrio)
      throw new Error("Different comparison functions")
   if (  queue.prioheap !== undefined
         && (  this.prioheap === undefined
            || this.higherPrio(queue.prioheap.value, this.prioheap.value)))
      this.prioheap = queue.prioheap
   this.size += queue.size
   while (queue.heaps.length) {
      const subheap = queue.heaps.pop()
      if (subheap !== undefined) {
         addHeap(this, subheap.logsize, subheap)
      }
   }
   queue.prioheap = undefined
   queue.size = 0
}

// action: removes all elements from queue in one step
// returns: undefined
PrioQueue.prototype.clear = function(queue) {
   this.heaps.length = 0
   this.prioheap = undefined
   this.size = 0
}

// returns: comparison function used to determine priority of values; signature (v1,v2) => { if "v1 has higher priority than v2" return true else return false }
Object.defineProperty(PrioQueue.prototype, "comparison", {
   get: function() {
      return this.higherPrio
   }
})

// returns: value with highest priority of all values stored in queue or undefined if queue is empty
Object.defineProperty(PrioQueue.prototype, "val", {
   get: function() {
      if (this.prioheap === undefined)
         return undefined
      return this.prioheap.value
   }
})

// returns: size of queue which is the number of contained values
Object.defineProperty(PrioQueue.prototype, "length", {
   get: function() { return this.size }
})

// return PrioQueue }() // PrioQueue namespace ?

/////////////////
// Module Test //
/////////////////

// param TEST: reference to TEST function -- see test.js
// action: tests public interface of PrioQueue
function unittest_prioqueue(TEST) {

   test_comparison()
   test_empty_queue(new PrioQueue())
   test_insert_remove_different_sizes()
   test_merge_fail()
   test_merge_different_sizes()
   test_clear()

   //////////////////////
   // helper functions //
   //////////////////////

   function test_comparison()
   {
      TEST([new PrioQueue().comparison],"==",[defaultComparison],"defaultComparison used as default comparison function")
      const dummyCmp = (v1,v2) => false;
      TEST([new PrioQueue(dummyCmp).comparison],"==",[dummyCmp],"comparison returns comparison function provided in constructor")
   }

   function test_empty_queue(queue)
   {
      // test empty with internal state
      TEST(queue.size,"==",0,"empty queue has size 0")
      TEST(queue.prioheap,"==",undefined,"empty queue has no heap with highest priority")
      TEST(queue.heaps.length,"==",0,"empty queue has no heaps")

      // test empty with type interface
      for (var i=0; i<2; ++i) {
         TEST(queue.length,"==",0,"empty queue has length 0")
         TEST(queue.val,"==",undefined,"val returns undefined if queue empty")
         TEST(queue.remove(),"==",undefined,"remove returns undefined if queue empty")
      }
   }

   function test_insert_remove_different_sizes()
   {
      var minqueue = new PrioQueue()
      var maxqueue = new PrioQueue((v1,v2) => v1>v2)

      for (var size=1; size<=1024; ++size) {
         test_insert_remove(minqueue, size, {ascending: true})
         test_insert_remove(maxqueue, size, {ascending: false})
      }

   }

   function test_insert_remove(queue, size, {ascending})
   {
      test_empty_queue(queue)

      for (var i=0; i<size; ++i)
      {
         TEST(queue.insert(i),"==",queue,"insert returns this")
         TEST(queue.length,"==",i+1,"length was incremented after insert")
         TEST(queue.val,"==",(ascending?0:i),"val returns (new) value with highest priority")
      }

      for (var i=0; i<size; ++i) {
         TEST(queue.val,"==",(ascending?i:size-1-i),"val returns value with highest priority after remove")
         TEST(queue.remove(),"==",(ascending?i:size-1-i),"remove returns value with highest priority")
         TEST(queue.length,"==",size-1-i,"length was decremented after remove")
      }

      test_empty_queue(queue)
   }

   function test_merge_fail()
   {
      var queue=new PrioQueue((v1,v2)=>v1<v2)
      var queue2=new PrioQueue((v1,v2)=>v1>v2)

      TEST(()=>queue.merge(queue2),"throw","Different comparison functions","merging only allowed with same comparison function")

      for (var i=0; i<7; ++i)
         queue.insert(i)
      queue.merge(queue)
      TEST(queue.length,"==",7,"merging with itself is a no-op")
      for (var i=0; i<7; ++i)
         TEST(queue.remove(),"==",i,"merging with itself is a no-op")
      test_empty_queue(queue)
   }

   function test_merge_different_sizes()
   {
      const maxComparison=(v1,v2) => v1>v2
      function newMinQueue() { return new PrioQueue() }
      function newMaxQueue() { return new PrioQueue(maxComparison) }

      var queue=newMinQueue(), queue2=newMinQueue();
      for (var size=0; size<256; size+=5)
         for (var size2=0; size2<256; size2+=5)
            test_merge(size, queue, size2, queue2, {ascending:true})
      queue=newMaxQueue(); queue2=newMaxQueue();
      for (var size=0; size<256; size+=5)
         for (var size2=0; size2<256; size2+=5)
            test_merge(size, queue, size2, queue2, {ascending:false})
   }

   function test_merge(size, queue, size2, queue2, {ascending}) {
      for (var swap=0; swap<=1; ++swap) {
         for (var i=0; i<size; ++i) {
            queue.insert(i)
         }
         for (var i=0; i<size2; ++i) {
            queue2.insert(size+i)
         }
         if (swap == 1) {
            [queue, queue2] = [queue2, queue]
         }
         queue.merge(queue2)
         test_empty_queue(queue2)
         for (var i=0; i<size+size2; ++i) {
            TEST(queue.length,"==",size+size2-i,"merge increments size of queue")
            TEST(queue.val,"==",(ascending?i:size+size2-1-i),"val returns value with highest priority")
            TEST(queue.remove(),"==",(ascending?i:size+size2-1-i),"remove returns value with highest priority")
         }
         test_empty_queue(queue)
      }
   }

   function test_clear() {
      var queue = new PrioQueue(), queue2 = new PrioQueue()

      // test: clear of empty queue
      queue.clear()
      test_empty_queue(queue)

      // test: clear of non-empty queue
      for (var i=0; i<15; ++i) {
         queue.insert(i)
         queue2.insert(10+i)
      }
      queue.clear()
      test_empty_queue(queue)

      // test: clear has cleared array of heaps
      queue.merge(queue2)
      for (var i=0; i<15; ++i) {
         TEST(queue.remove(),"==",10+i,"expect next value only from merged queue2 not from cleared queue")
      }
      test_empty_queue(queue)
   }

}

////////////
// export //
////////////

export {
   PrioQueue,  // constructor of implemented priority queue
   unittest_prioqueue as unittest, // unit test function of this module
}
