# Generic tree

Each line of standard input is `int` or `str` followed by zero or more values separated by single spaces: integers after `int`, words of ASCII letters after `str`.

For each line, start from an empty binary search tree and insert the values in input order: a value smaller than a node's value goes into its left subtree, a larger one into its right subtree, and a value equal to one already in the tree is ignored. Print

```
h=HEIGHT V1 V2 ...
```

where `HEIGHT` is the number of nodes on the longest path from the root to a leaf (0 for an empty tree, then just `h=0`) and `V1 V2 ...` are the values in in-order (ascending) order. Strings compare by byte order.

Define the tree as one generic variant (an empty tree or a node) with generic functions for insert, height and in-order traversal, used for both integer and string trees.
