#if !defined(_huffdec_H)
# define _huffdec_H (1)
# include "huffman.h"



typedef struct oc_huff_node oc_huff_node;

/*A node in the Huffman tree.
  Instead of storing every branching in the tree, subtrees can be collapsed
   into one node, with a table of size 1<<nbits pointing directly to its
   descedents nbits levels down.
  This allows more than one bit to be read at a time, and avoids following all
   the intermediate branches with next to no increased code complexity once
   the collapsed tree has been built.
  We do _not_ require that a subtree be complete to be collapsed, but instead
   store duplicate pointers in the table, and record the actual depth of the
   node below its parent.
  This tells us the number of bits to advance the stream after reaching it.*/
struct oc_huff_node{
  /*The number of bits of the code needed to descend through this node.
    0 indicates a leaf node.
    Otherwise there are 1<<nbits nodes in the nodes table, which can be
     indexed by reading nbits bits from the stream.*/
  unsigned char  nbits;
  /*The value of a token stored in a leaf node.
    The value in non-leaf nodes is undefined.*/
  unsigned char  token;
  /*The depth of the current node, relative to its parent in the collapsed
     tree.
    This can be less than its parent's nbits value, in which case there are
     1<<nbits-depth copies of this node in the table, and the bitstream should
     only be advanced depth bits after reaching this node.*/
  unsigned char  depth;
  /*The table of child nodes.
    The ACTUAL size of this array is 1<<nbits, despite what the declaration
     below claims.
    The exception is that for leaf nodes the size is 0.*/
  oc_huff_node  *nodes[1];
};



int oc_huff_trees_unpack(oggpack_buffer *_opb,
 oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]);
void oc_huff_trees_copy(oc_huff_node *_dst[TH_NHUFFMAN_TABLES],
 const oc_huff_node *const _src[TH_NHUFFMAN_TABLES]);
void oc_huff_trees_clear(oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]);
int oc_huff_token_decode(oggpack_buffer *_opb,const oc_huff_node *_node);


#endif
