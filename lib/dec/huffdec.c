/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <ogg/ogg.h>
#include "huffdec.h"
#include "decint.h"


/*The ANSI offsetof macro is broken on some platforms (e.g., older DECs).*/
#define _ogg_offsetof(_type,_field)\
 ((size_t)((char *)&((_type *)0)->_field-(char *)0))

/*The number of internal tokens associated with each of the spec tokens.*/
static const unsigned char OC_DCT_TOKEN_MAP_ENTRIES[TH_NDCT_TOKENS]={
  1,1,1,4,8,1,1,8,1,1,1,1,1,2,2,2,2,4,8,2,2,2,4,2,2,2,2,2,8,2,4,8
};

/*The map from external spec-defined tokens to internal tokens.
  This is constructed so that any extra bits read with the original token value
   can be masked off the least significant bits of its internal token index.
  In addition, all of the tokens which require additional extra bits are placed
   at the start of the list, and grouped by type.
  These requirements leave things slightly out of order, and leave a few gaps.*/
static const unsigned char OC_DCT_TOKEN_MAP[TH_NDCT_TOKENS][8]={
  /*OC_DCT_EOB1_TOKEN (0 extra bits)*/
  {17},
  /*OC_DCT_EOB2_TOKEN (0 extra bits)*/
  {18},
  /*OC_DCT_EOB3_TOKEN (0 extra bits)*/
  {19},
  /*OC_DCT_REPEAT_RUN0_TOKEN (2 extra bits)*/
  {20,21,22,23},
  /*OC_DCT_REPEAT_RUN1_TOKEN (3 extra bits)*/
  {24,25,26,27,28,29,30,31},
  /*OC_DCT_REPEAT_RUN2_TOKEN (4 extra bits)*/
  {1},
  /*OC_DCT_REPEAT_RUN3_TOKEN (12 extra bits)*/
  {0},
  /*OC_DCT_SHORT_ZRL_TOKEN (3 extra bits)*/
  {32,33,34,35,36,37,38,39},
  /*OC_DCT_ZRL_TOKEN (6 extra bits)*/
  {4},
  /*OC_ONE_TOKEN (0 extra bits)*/
  {40},
  /*OC_MINUS_ONE_TOKEN (0 extra bits)*/
  {41},
  /*OC_TWO_TOKEN (0 extra bits)*/
  {42},
  /*OC_MINUS_TWO_TOKEN (0 extra bits)*/
  {43},
  /*OC_DCT_VAL_CAT2 (1 extra bit)*/
  {44,45},
  {46,47},
  {48,49},
  {50,51},
  /*OC_DCT_VAL_CAT3 (2 extra bits)*/
  {52,53,54,55},
  /*OC_DCT_VAL_CAT4 (3 extra bits)*/
  {56,57,58,59,60,61,62,63},
  /*OC_DCT_VAL_CAT5 (4 extra bits)*/
  {6,7},
  /*OC_DCT_VAL_CAT6 (5 extra bits)*/
  {8,9},
  /*OC_DCT_VAL_CAT7 (6 extra bits)*/
  {10,11},
  /*OC_DCT_VAL_CAT8 (10 extra bits)*/
  {12,13,14,15},
  /*OC_DCT_RUN_CAT1A (1 extra bit)*/
  {84,85},
  {86,87},
  {88,89},
  {90,91},
  {92,93},
  /*OC_DCT_RUN_CAT1B (3 extra bits)*/
  {64,65,66,67,68,69,70,71},
  /*OC_DCT_RUN_CAT1C (4 extra bits)*/
  {2,3},
  /*OC_DCT_RUN_CAT2A (2 extra bits)*/
  {80,81,82,83},
  /*OC_DCT_RUN_CAT2B (3 extra bits)*/
  {72,73,74,75,76,77,78,79}
};

/*These three functions are really part of the bitpack.c module, but
   they are only used here.
  Declaring local static versions so they can be inlined saves considerable
   function call overhead.*/

static oc_pb_window oc_pack_refill(oc_pack_buf *_b,int _bits){
  const unsigned char *ptr;
  const unsigned char *stop;
  oc_pb_window         window;
  int                  available;
  window=_b->window;
  available=_b->bits;
  ptr=_b->ptr;
  stop=_b->stop;
  /*This version of _refill() doesn't bother setting eof because we won't
     check for it after we've started decoding DCT tokens.*/
  if(ptr>=stop)available=OC_LOTS_OF_BITS;
  while(available<=OC_PB_WINDOW_SIZE-8){
    available+=8;
    window|=(oc_pb_window)*ptr++<<OC_PB_WINDOW_SIZE-available;
    if(ptr>=stop)available=OC_LOTS_OF_BITS;
  }
  _b->ptr=ptr;
  if(_bits>available)window|=*ptr>>(available&7);
  _b->bits=available;
  return window;
}


/*Read in bits without advancing the bit pointer.
  Here we assume 0<=_bits&&_bits<=32.*/
static long oc_pack_look(oc_pack_buf *_b,int _bits){
  oc_pb_window window;
  int          available;
  long         result;
  window=_b->window;
  available=_b->bits;
  if(_bits==0)return 0;
  if(_bits>available)_b->window=window=oc_pack_refill(_b,_bits);
  result=window>>OC_PB_WINDOW_SIZE-_bits;
  return result;
}

/*Advance the bit pointer.*/
static void oc_pack_adv(oc_pack_buf *_b,int _bits){
  /*We ignore the special cases for _bits==0 and _bits==32 here, since they are
     never used actually used.
    OC_HUFF_SLUSH (defined below) would have to be at least 27 to actually read
     32 bits in a single go, and would require a 32 GB lookup table (assuming
     8 byte pointers, since 4 byte pointers couldn't fit such a table).*/
  _b->window<<=_bits;
  _b->bits-=_bits;
}


/*The log_2 of the size of a lookup table is allowed to grow to relative to
   the number of unique nodes it contains.
  E.g., if OC_HUFF_SLUSH is 2, then at most 75% of the space in the tree is
   wasted (each node will have an amortized cost of at most 20 bytes when using
   4-byte pointers).
  Larger numbers can decode tokens with fewer read operations, while smaller
   numbers may save more space (requiring as little as 8 bytes amortized per
   node, though there will be more nodes).
  With a sample file:
  32233473 read calls are required when no tree collapsing is done (100.0%).
  19269269 read calls are required when OC_HUFF_SLUSH is 0 (59.8%).
  11144969 read calls are required when OC_HUFF_SLUSH is 1 (34.6%).
  10538563 read calls are required when OC_HUFF_SLUSH is 2 (32.7%).
  10192578 read calls are required when OC_HUFF_SLUSH is 3 (31.6%).
  Since a value of 1 gets us the vast majority of the speed-up with only a
   small amount of wasted memory, this is what we use.*/
#define OC_HUFF_SLUSH (1)


/*Allocates a Huffman tree node that represents a subtree of depth _nbits.
  _nbits: The depth of the subtree.
          If this is 0, the node is a leaf node.
          Otherwise 1<<_nbits pointers are allocated for children.
  Return: The newly allocated and fully initialized Huffman tree node.*/
static oc_huff_node *oc_huff_node_alloc(int _nbits){
  oc_huff_node *ret;
  size_t        size;
  size=_ogg_offsetof(oc_huff_node,nodes);
  if(_nbits>0)size+=sizeof(oc_huff_node *)*(1<<_nbits);
  ret=_ogg_calloc(1,size);
  ret->nbits=(unsigned char)_nbits;
  return ret;
}

/*Frees a Huffman tree node allocated with oc_huf_node_alloc.
  _node: The node to free.
         This may be NULL.*/
static void oc_huff_node_free(oc_huff_node *_node){
  _ogg_free(_node);
}

/*Frees the memory used by a Huffman tree.
  _node: The Huffman tree to free.
         This may be NULL.*/
static void oc_huff_tree_free(oc_huff_node *_node){
  if(_node==NULL)return;
  if(_node->nbits){
    int nchildren;
    int i;
    int inext;
    nchildren=1<<_node->nbits;
    for(i=0;i<nchildren;i=inext){
      inext=i+(_node->nodes[i]!=NULL?1<<_node->nbits-_node->nodes[i]->depth:1);
      oc_huff_tree_free(_node->nodes[i]);
    }
  }
  oc_huff_node_free(_node);
}

/*Unpacks a sub-tree from the given buffer.
  _opb:      The buffer to unpack from.
  _binodes:  The nodes to store the sub-tree in.
  _nbinodes: The number of nodes available for the sub-tree.
  Return: 0 on success, or a negative value on error.*/
static int oc_huff_tree_unpack(oc_pack_buf *_opb,
 oc_huff_node *_binodes,int _nbinodes){
  oc_huff_node *binode;
  long          bits;
  int           nused;
  if(_nbinodes<1)return TH_EBADHEADER;
  binode=_binodes;
  nused=0;
  bits=oc_pack_read1(_opb);
  if(oc_pack_bytes_left(_opb)<0)return TH_EBADHEADER;
  /*Read an internal node:*/
  if(!bits){
    int ret;
    nused++;
    binode->nbits=1;
    binode->depth=1;
    binode->nodes[0]=_binodes+nused;
    ret=oc_huff_tree_unpack(_opb,_binodes+nused,_nbinodes-nused);
    if(ret>=0){
      nused+=ret;
      binode->nodes[1]=_binodes+nused;
      ret=oc_huff_tree_unpack(_opb,_binodes+nused,_nbinodes-nused);
    }
    if(ret<0)return ret;
    nused+=ret;
  }
  /*Read a leaf node:*/
  else{
    int ntokens;
    int i;
    bits=oc_pack_read(_opb,OC_NDCT_TOKEN_BITS);
    if(oc_pack_bytes_left(_opb)<0)return TH_EBADHEADER;
    /*Find out how many internal tokens we translate this external token into.*/
    ntokens=OC_DCT_TOKEN_MAP_ENTRIES[bits];
    if(_nbinodes<2*ntokens-1)return TH_EBADHEADER;
    /*Fill in a complete binary tree pointing to the internal tokens.*/
    for(i=1;i<ntokens;i<<=1){
      int j;
      binode=_binodes+nused;
      nused+=i;
      for(j=0;j<i;j++){
        binode[j].nbits=1;
        binode[j].depth=1;
        binode[j].nodes[0]=_binodes+nused+2*j;
        binode[j].nodes[1]=_binodes+nused+2*j+1;
      }
    }
    /*And now the leaf nodes with those tokens.*/
    for(i=0;i<ntokens;i++){
      binode=_binodes+nused++;
      binode->nbits=0;
      binode->depth=1;
      binode->token=OC_DCT_TOKEN_MAP[bits][i];
    }
  }
  return nused;
}

/*Finds the depth of shortest branch of the given sub-tree.
  The tree must be binary.
  _binode: The root of the given sub-tree.
           _binode->nbits must be 0 or 1.
  Return: The smallest depth of a leaf node in this sub-tree.
          0 indicates this sub-tree is a leaf node.*/
static int oc_huff_tree_mindepth(oc_huff_node *_binode){
  int depth0;
  int depth1;
  if(_binode->nbits==0)return 0;
  depth0=oc_huff_tree_mindepth(_binode->nodes[0]);
  depth1=oc_huff_tree_mindepth(_binode->nodes[1]);
  return OC_MINI(depth0,depth1)+1;
}

/*Finds the number of internal nodes at a given depth, plus the number of
   leaves at that depth or shallower.
  The tree must be binary.
  _binode: The root of the given sub-tree.
           _binode->nbits must be 0 or 1.
  Return: The number of entries that would be contained in a jump table of the
           given depth.*/
static int oc_huff_tree_occupancy(oc_huff_node *_binode,int _depth){
  if(_binode->nbits==0||_depth<=0)return 1;
  else{
    return oc_huff_tree_occupancy(_binode->nodes[0],_depth-1)+
     oc_huff_tree_occupancy(_binode->nodes[1],_depth-1);
  }
}

/*Makes a copy of the given Huffman tree.
  _node: The Huffman tree to copy.
  Return: The copy of the Huffman tree.*/
static oc_huff_node *oc_huff_tree_copy(const oc_huff_node *_node){
  oc_huff_node *ret;
  ret=oc_huff_node_alloc(_node->nbits);
  ret->depth=_node->depth;
  if(_node->nbits){
    int nchildren;
    int i;
    int inext;
    nchildren=1<<_node->nbits;
    for(i=0;i<nchildren;){
      ret->nodes[i]=oc_huff_tree_copy(_node->nodes[i]);
      inext=i+(1<<_node->nbits-ret->nodes[i]->depth);
      while(++i<inext)ret->nodes[i]=ret->nodes[i-1];
    }
  }
  else ret->token=_node->token;
  return ret;
}

static oc_huff_node *oc_huff_tree_collapse(oc_huff_node *_binode);

/*Fills the given nodes table with all the children in the sub-tree at the
   given depth.
  The nodes in the sub-tree with a depth less than that stored in the table
   are freed.
  The sub-tree must be binary and complete up until the given depth.
  _nodes:  The nodes table to fill.
  _binode: The root of the sub-tree to fill it with.
           _binode->nbits must be 0 or 1.
  _level:  The current level in the table.
           0 indicates that the current node should be stored, regardless of
            whether it is a leaf node or an internal node.
  _depth:  The depth of the nodes to fill the table with, relative to their
            parent.*/
static void oc_huff_node_fill(oc_huff_node **_nodes,
 oc_huff_node *_binode,int _level,int _depth){
  if(_level<=0||_binode->nbits==0){
    int i;
    _binode->depth=(unsigned char)(_depth-_level);
    _nodes[0]=oc_huff_tree_collapse(_binode);
    for(i=1;i<1<<_level;i++)_nodes[i]=_nodes[0];
  }
  else{
    _level--;
    oc_huff_node_fill(_nodes,_binode->nodes[0],_level,_depth);
    oc_huff_node_fill(_nodes+(1<<_level),_binode->nodes[1],_level,_depth);
  }
}

/*Finds the largest complete sub-tree rooted at the current node and collapses
   it into a single node.
  This procedure is then applied recursively to all the children of that node.
  _binode: The root of the sub-tree to collapse.
           _binode->nbits must be 0 or 1.
  Return: The new root of the collapsed sub-tree.*/
static oc_huff_node *oc_huff_tree_collapse(oc_huff_node *_binode){
  oc_huff_node *root;
  int           mindepth;
  int           depth;
  int           loccupancy;
  int           occupancy;
  depth=mindepth=oc_huff_tree_mindepth(_binode);
  occupancy=1<<mindepth;
  do{
    loccupancy=occupancy;
    occupancy=oc_huff_tree_occupancy(_binode,++depth);
  }
  while(occupancy>loccupancy&&occupancy>=1<<OC_MAXI(depth-OC_HUFF_SLUSH,0));
  depth--;
  if(depth<=1)return oc_huff_tree_copy(_binode);
  root=oc_huff_node_alloc(depth);
  root->depth=_binode->depth;
  oc_huff_node_fill(root->nodes,_binode,depth,depth);
  return root;
}

/*Unpacks a set of Huffman trees, and reduces them to a collapsed
   representation.
  _opb:   The buffer to unpack the trees from.
  _nodes: The table to fill with the Huffman trees.
  Return: 0 on success, or a negative value on error.*/
int oc_huff_trees_unpack(oc_pack_buf *_opb,
 oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++){
    oc_huff_node nodes[511];
    int          ret;
    /*Unpack the full tree into a temporary buffer.*/
    ret=oc_huff_tree_unpack(_opb,nodes,sizeof(nodes)/sizeof(*nodes));
    if(ret<0)return ret;
    _nodes[i]=oc_huff_tree_collapse(nodes);
  }
  return 0;
}

/*Makes a copy of the given set of Huffman trees.
  _dst: The array to store the copy in.
  _src: The array of trees to copy.*/
void oc_huff_trees_copy(oc_huff_node *_dst[TH_NHUFFMAN_TABLES],
 const oc_huff_node *const _src[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++)_dst[i]=oc_huff_tree_copy(_src[i]);
}

/*Frees the memory used by a set of Huffman trees.
  _nodes: The array of trees to free.*/
void oc_huff_trees_clear(oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++)oc_huff_tree_free(_nodes[i]);
}

/*Unpacks a single token using the given Huffman tree.
  _opb:  The buffer to unpack the token from.
  _node: The tree to unpack the token with.
  Return: The token value.*/
int oc_huff_token_decode(oc_pack_buf *_opb,const oc_huff_node *_node){
  long bits;
  while(_node->nbits!=0){
    bits=oc_pack_look(_opb,_node->nbits);
    _node=_node->nodes[bits];
    oc_pack_adv(_opb,_node->depth);
  }
  return _node->token;
}
