/* Flow module which manages dependencies of data flows (see "Data Flow Diagrams") between processing steps.
   (c) 2022 Jörg Seebohn */

class Graph {
   constructor(getNodeIndex=(node => node.nr)) {
      this.nodes=[] // stores Node indexed by [0..nodeCount-1]
      this.getNodeIndex=getNodeIndex // getNodeIndex(node) returns node index starting from 0
   }
   /** Returns a new graph node.
     * A new one is created if Node is called the first time for argument node. */
   Node(node) {
      const ni=this.getNodeIndex(node)
      let graphNode=this.nodes[ni]
      if (graphNode === undefined) {
         graphNode={ node,
            nextNodes: [], // nodes which depend on this node
            nrPrevNodes: 0, // number of nodes this node depends on
            nrVisitedFromPrev: 0, // number of nodes followed dependency and reached this node
            order: 0,
         }
         this.nodes[ni]=graphNode
      }
      return graphNode
   }
   addDependency(toNode,fromNodes) {
      const toGraphNode=this.Node(toNode)
      toGraphNode.nrPrevNodes+=fromNodes.length
      fromNodes.forEach( fromNode => this.Node(fromNode).nextNodes.push(toGraphNode))
   }
   /** Tests for cycles and returns a random one if there is at least one else undefined. */
   get cycle() {
      let cycle=undefined
      this.nodes.forEach( n => n.visit=(n.nrVisitedFromPrev!==n.nrPrevNodes ? 2 : 0)) // nodes which may be part of cycle are marked with visit=2
      const visit=(node) => { // depth first search
         if (node.visit===2 /*unvisited*/) {
            node.visit=1 // state: visiting
            if (node.nextNodes.find(visit))
               cycle.unshift(node.node)
            node.visit=0 // state: done
         }
         else if (node.visit===1 /*visiting*/)
            cycle=[node.node]
         return cycle
      }
      if (this.nodes.find(visit))
         while (cycle[0]!==cycle[cycle.length-1])
            cycle.shift() // remove start node cause cycle starts and ends with same node (end node is part of cycle)
      return cycle
   }
   determineOrder(processOrderedNode) {
      // breadth first traversal to determine order of nodes (from input nodes (source) to output nodes (sink))
      let visitNodes=this.nodes.filter( n => n.nrPrevNodes===0) // start with input nodes
      let order=-1, nrVisited=0
      while ((++order, nrVisited+=visitNodes.length, visitNodes.length>0))
         visitNodes=visitNodes.flatMap( node => (node.order=order, node.nextNodes.filter( next => ++next.nrVisitedFromPrev === next.nrPrevNodes)))
      if (nrVisited!==this.nodes.length)
         return this.cycle
      // process the result
      this.nodes.forEach( node => processOrderedNode(node.node,node.order))
   }
}

// test code

var nodes=[ {nr:0}, {nr:1}, {nr:2}, {nr:3} ]
var graph=new Graph(nodeIndex=>nodeIndex)
// graph.addDependency(1,[0,2,3,3])
graph.addDependency(1,[0])
graph.addDependency(2,[1])
graph.addDependency(3,[2])
// graph.addDependency(3,[3])
// graph.addDependency(1,[3])
let cycle=graph.determineOrder( (nodeIndex,order) => nodes[nodeIndex].order=order )
console.log(cycle)
console.log(graph.cycle)

nodes.sort( (n1,n2) => n1.order-n2.order)
console.log("ordered nodes",nodes)
